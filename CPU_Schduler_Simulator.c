#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <memory.h>
#include <string.h>
#define BUFFER_SIZE 5


typedef struct
{
    int pid;
    int burst_time;
    int IO_burst_time;
    int arrival_time;
    int interrupt_time;
    float priority;
} process;
typedef struct 
{
    process* processes[BUFFER_SIZE];
    int end;
} queue;
typedef struct record
{
    struct record* next;
    int i;
    int start;
    int end;
} record;
typedef struct 
{
    record* first;
    record* last;
} records;

void Create_Process(process* processes);
void Init_queue(queue* q);
void push_queue(queue* q, process* p);
void pop_queue(queue* q, process** p);
void init_records(records* recs);
record* make_record(int i, int start, int end);
void push_record(records* recs, int i, int start, int end);
void FCFS(queue* ready_queue);
void SJF(queue* q);
void Priority(queue* q);
void RR(queue* q);
void LIFO(queue* q);
void Aging(queue* q, int timer);
void Scheduler(process* processes, queue* ready_queue, queue* waiting_queue, void (*pf)(queue*), int time_slice, bool preemptive, bool aging, int* turnaround, records* recs_cpu, records* recs_IO);
void Gantt_Chart(records* recs);
void Visualize(process* processes, int turnaround, records* recs_cpu, records* recs_IO, int* avg_turn, int* avg_wait);
void Evaluation(char (*func)[30], int* avg_turn, int num);
void Find_Best_Scheduling(char func[20][30], void (*fp[20])(queue*), int time_slice[20], bool preemt[20], bool aging[20]);

int main() {
    srand(time(NULL));
    process processes[BUFFER_SIZE];
    queue ready_queue, waiting_queue;
    int turnaround;
    records recs_cpu, recs_IO;
    Init_queue(&ready_queue);
    Init_queue(&waiting_queue);

    Create_Process(processes);
    printf("P\tpid\tburst_time\tIO_burst_time\tarrival_time\tinterrupt_time\tpriority\n");
    for (int i = 0; i < BUFFER_SIZE; i++)
        printf("P%d\t%d\t%d\t\t%d\t\t%d\t\t%d\t\t%0.0f\n", 
        i, processes[i].pid, processes[i].burst_time, processes[i].IO_burst_time, 
        processes[i].arrival_time, processes[i].interrupt_time, processes[i].priority);
    
    char func[20][30] = {"FCFS", "SJF", "Priority", "Preemptive SJF", "Preemptive Priority", "RR(5)-FCFS", "RR(10)-FCFS", "RR(5)-Priority", "LIFO", "Preemptive LIFO", "Preemptive Priority-Aging"};
    void (*fp[20])(queue*) = {FCFS, SJF, Priority, SJF, Priority, FCFS, FCFS, Priority, LIFO, LIFO, Priority};
    int time_slice[20] = {1000,1000,1000,1000,1000,5,10,5, 1000, 1000, 1000};
    bool preemt[20] = {false, false, false, true, true, false, false, false, false, true, true};
    bool aging[20] = {false, false, false, false, false, false, false, false, false, false, true};
    int avg_turn[20], avg_wait[20] = {0};

    for (int i = 0; i < 11; i++){
        printf("%s\n", func[i]);
        Scheduler(processes, &ready_queue, &waiting_queue, fp[i], time_slice[i], preemt[i], false, &turnaround, &recs_cpu, &recs_IO);
        Visualize(processes, turnaround, &recs_cpu, &recs_IO, avg_turn+i, avg_wait+i);
    }
    
    Evaluation(func, avg_turn, 11);
    Find_Best_Scheduling(func, fp, time_slice, preemt, aging);
    return 0;
}

void Create_Process(process* processes){
    while(1){
        int pid_dup = 0;
        for (int i = 0; i < BUFFER_SIZE; i++){
            processes[i].pid = rand() % 1000;
            processes[i].burst_time = rand() % 20 + 2;
            processes[i].IO_burst_time = rand() % 20 + 1;
            processes[i].arrival_time = rand() % 20;
            processes[i].interrupt_time = processes[i].burst_time / 2;
            processes[i].priority = rand() % 100;
        }
        for (int i = 0; i < 5; i++){
            for (int j = 0; j <5; j++){
                if (i == j) continue;
                if (processes[i].pid == processes[j].pid)
                    pid_dup = 1;
            }
        }
        if (!pid_dup) break;
    }
}

void Init_queue(queue* q) {
    for (int i = 0; i < BUFFER_SIZE; i++)
        q->processes[i] = 0;
    q->end = 0;
}
void push_queue(queue* q, process* p) {
    q->processes[q->end] = p;
    q->end += 1;
}
void pop_queue(queue* q, process** p) {
    if (q->end == 0) {
        *p = 0;
        return;
    }
    *p = q->processes[0];
    for (int i = 1; i < q->end; i++){
        q->processes[i-1] = q->processes[i];
    }
    q->end -= 1;
}
void init_records(records* recs) {
    recs->first = NULL;
    recs->last = NULL;
}
record* make_record(int i, int start, int end) {
    record* new = (record*)malloc(sizeof(record));
    new->i = i;
    new->start = start;
    new->end = end;
    new->next = NULL;
    return new;
}
void push_record(records* recs, int i, int start, int end) {
    record* new = make_record(i, start, end);
    if (recs->last == NULL) {
        recs->first = new;
        recs->last = new;
    }
    else {
        recs->last->next = new;
        recs->last = new;
    }
}

void Scheduler(process* processes, queue* ready_queue, queue* waiting_queue, void (*pf)(queue*), int time_slice, bool preemptive, bool aging, int* turnaround, records* recs_cpu, records* recs_IO) {
    bool complete[BUFFER_SIZE] = {0,};
    bool finish = false;
    int timer = 0, start = 0, start_io = 0;
    process* assigned = 0, *assigned_io = 0;
    process copy[BUFFER_SIZE];
    *turnaround = 0;
    init_records(recs_cpu);
    init_records(recs_IO);
    for (int i = 0; i < BUFFER_SIZE; i++){
        copy[i] = processes[i];
    }
    while(1) {
        // 모든 프로세스가 완료될 시 중지
        for (int i = 0; i < BUFFER_SIZE; i++) {
            if (complete[i] == false) break;
            if (i == BUFFER_SIZE-1) finish = true;
        }
        if (finish) break;
        
        // arrival time일 시 ready_queue push
        for (int i = 0; i < BUFFER_SIZE; i++){
            if (copy[i].arrival_time == timer){
                push_queue(ready_queue, copy+i);
                (*pf)(ready_queue); // 정렬
            }
        }

        // CPU가 할당된 프로세스가 종료될 때
        if (assigned != 0 && assigned->burst_time == timer-start) {
            int i = assigned-copy;
            complete[i] = true;
            *turnaround  += timer - processes[i].arrival_time;
            push_record(recs_cpu, i, start, timer);
            start = timer;
            assigned = 0;
        }
        // 할당된 프로세스 작동 중 IO interrupt 발생 시
        else if (assigned != 0 && assigned->interrupt_time == timer-start) {
            int i = assigned-copy;
            assigned->burst_time -= assigned->interrupt_time;
            assigned->interrupt_time += assigned->burst_time;
            push_queue(waiting_queue, assigned);
            push_record(recs_cpu, i, start, timer);
            start = timer;
            assigned = 0;

        }
        // Round Robin일 경우. 아닐 때는 RR이 매우 커서 고려할 필요X
        else if (assigned !=0 && time_slice == timer-start) {
            int i = assigned-copy;
            assigned->burst_time -= time_slice;
            assigned->interrupt_time -= time_slice;
            push_queue(ready_queue, assigned);
            push_record(recs_cpu, i, start, timer);
            RR(ready_queue);
            start = timer;
            assigned = 0;
        }

        // IO 작업 중이던 것이 완료될 때. 
        if (assigned_io != 0 && assigned_io->IO_burst_time == timer-start_io) {
            int i = assigned_io-copy;
            assigned_io->arrival_time = timer;
            push_queue(ready_queue, assigned_io);
            push_record(recs_IO, i, start_io, timer);
            (*pf)(ready_queue);
            start_io = timer;
            assigned_io = 0;
        }
        if (aging) Aging(ready_queue, timer);
//////////////////////////////////////////// 할당 영역
        if (assigned_io == 0) { // assigning IO
            pop_queue(waiting_queue, &assigned_io);
            if (assigned_io != 0 && start_io != timer){
                push_record(recs_IO, -1, start_io, timer);
                start_io = timer;
            }
        }
        if (assigned == 0){ // assigning CPU
            pop_queue(ready_queue, &assigned);
            if (assigned != 0 && start != timer){
                push_record(recs_cpu, -1, start, timer);
                start = timer;
            }
        }
        //////////////////////////////////// preemptive == true
        else if (preemptive && assigned->priority > ready_queue->processes[0]->priority) {
            int i = assigned - copy;
            assigned->burst_time -= timer-start;
            assigned->interrupt_time -= timer-start;
            push_queue(ready_queue, assigned);
            pop_queue(ready_queue, &assigned);
            (*pf)(ready_queue);
            push_record(recs_cpu, i, start, timer);
            start = timer;
        }
        
        timer++;
    }
}

void FCFS(queue* q) {
    q->processes[q->end-1]->priority = q->processes[q->end-1]->arrival_time;
    Priority(q);
}
void SJF(queue* q) {
    q->processes[q->end-1]->priority = q->processes[q->end-1]->burst_time;
    Priority(q);
}
void Priority(queue* q) {
    for (int i = 0; i < q->end; i++){
        for (int j = i+1; j < q->end; j++){
            if (q->processes[i]->priority > q->processes[j]->priority) {
                process* tmp = q->processes[i];
                q->processes[i] = q->processes[j];
                q->processes[j] = tmp;
            }
        }
    }
}
void RR(queue* q) {
    if (q->end-1 != 0) {
        if (q->processes[q->end-2]->priority >= 100) // 100이상이면 RR에 의해 뒤로 밀려난 것. 그 뒤로 줄 서면 됨
            q->processes[q->end-1]->priority = q->processes[q->end-2]->priority + 1;    
        else 
            q->processes[q->end-1]->priority = 100; // 만약 맨 마지막이 100이하였으면 최초로 밀려난 것. 
    }
        
    Priority(q);
}
void LIFO(queue* q) {
    q->processes[q->end-1]->priority = -(q->processes[q->end-1]->arrival_time);
    Priority(q);
}

void Aging(queue* q, int timer) {
    for (int i = 0; i < q->end; i++){
        q->processes[i]->priority -= 0.1*(timer - q->processes[i]->arrival_time);
    }
}

void Visualize(process* processes, int turnaround, records* recs_cpu, records* recs_IO, int* avg_turn, int* avg_wait) {
    float sum_turn = turnaround;
    float sum_wait = 0;

    sum_wait = sum_turn;
    for (int i = 0; i < BUFFER_SIZE; i++)
        sum_wait -= processes[i].burst_time + processes[i].IO_burst_time;
    printf("CPU Gantt Chart\n");
    Gantt_Chart(recs_cpu);
    printf("IO Gantt Chart\n");
    Gantt_Chart(recs_IO);

    printf("Average turnaround time: %0.1f\n", sum_turn / BUFFER_SIZE);
    printf("Average waiting time: %0.1f\n\n", sum_wait / BUFFER_SIZE);
    *avg_turn = sum_turn/BUFFER_SIZE;
    *avg_wait = sum_wait/BUFFER_SIZE;    
}

void Gantt_Chart(records* recs) {
    record* cur = recs->first;
    while (cur != NULL) {
        if (cur->i == -1){
            if (cur->end != 0)
                printf("|  Nan  ");
        }
        else
            printf("|  P%d   ", cur->i);
        cur = cur->next;
    }
    printf("|\n");
    cur = recs->first;
    while (cur != NULL) {
        if (cur->i != -1 || cur->end != 0)
            printf("%d\t", cur->start);
        cur = cur->next;
    }
    printf("%d\n", recs->last->end);
    cur = recs->first;
    while(cur != NULL) {
        record* next = cur->next;
        free(cur);
        cur = next;
    }
}

void Evaluation(char (*func)[30], int* avg_turn, int num){
    char copy[20][30];
    for (int i = 0; i < num; i++){
        strcpy(copy[i], func[i]);
    } 
    for (int i = 0; i < num; i++){
        for (int j = i+1; j < num; j++){
            if (avg_turn[i] > avg_turn[j]){
                int tmp = avg_turn[i];
                char stmp[30];
                avg_turn[i] = avg_turn[j];
                avg_turn[j] = tmp;
                strcpy(stmp, copy[i]);
                strcpy(copy[i], copy[j]);
                strcpy(copy[j], stmp);                
            }
        }
    }
    printf("Result: Average turnaround time\n");
    for (int i = 0; i < num; i++) {
        if (i == num-1) printf("%s(%d)\n", copy[i], avg_turn[i]);
        else printf("%s(%d) < ", copy[i], avg_turn[i]);
    }
}

void Find_Best_Scheduling(char func[20][30], void (*fp[20])(queue*), int time_slice[20], bool preemt[20], bool aging[20]){
    queue ready_queue, waiting_queue;
    int turnaround[20], best[20] = {0,}, idx[20]={0,};
    process processes[BUFFER_SIZE];
    records recs_cpu, recs_IO;
    Init_queue(&ready_queue);
    Init_queue(&waiting_queue);
    
    for (int i = 0; i < 100000; i++){
        int min_idx = 0;
        Create_Process(processes);
        for (int j = 0; j < 11; j++){
            Scheduler(processes, &ready_queue, &waiting_queue, fp[j], time_slice[j], preemt[j], aging[j], turnaround+j, &recs_cpu, &recs_IO);
        }
        for (int j = 1; j < 11; j++){
            if (turnaround[min_idx] > turnaround[j]){
                min_idx = j;
            }
        }
        for (int j = 0; j < 11; j++){
            if (turnaround[j] == turnaround[min_idx]){
                best[j] += 1;
            }
        }
    }
    char copy[20][30];
    for (int i = 0; i < 11; i++){
        strcpy(copy[i], func[i]);
    } 
    for (int i = 0; i < 11; i++){
        for (int j = i+1; j < 11; j++){
            if (best[i] < best[j]){
                int tmp = best[i];
                char stmp[30];
                best[i] = best[j];
                best[j] = tmp;
                strcpy(stmp, copy[i]);
                strcpy(copy[i], copy[j]);
                strcpy(copy[j], stmp);                
            }
        }
    }
    printf("\niter: 100000\n");
    printf("# of times selected as best: ");
    for (int i = 0; i < 11; i++) {
        if (i == 10) printf("%s(%d)\n", copy[i], best[i]);
        else printf("%s(%d) > ", copy[i], best[i]);
    }
}