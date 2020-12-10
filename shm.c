#include "param.h"
#include "types.h"
#include "defs.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"

struct {
  struct spinlock lock;
  struct shm_page {
    uint id;
    char *frame;
    int refcnt;
  } shm_pages[64];
} shm_table;

void shminit() {
  int i;
  initlock(&(shm_table.lock), "SHM lock");
  acquire(&(shm_table.lock));
  for (i = 0; i< 64; i++) {
    shm_table.shm_pages[i].id =0;
    shm_table.shm_pages[i].frame =0;
    shm_table.shm_pages[i].refcnt =0;
  }
  release(&(shm_table.lock));
}

// Get a shared memory page by its id
// Returns a pointer to the shm_page or 0 if it could not be found
struct shm_page * shm_get_page(int id) {
  uint i;
  for (i = 0; i < 64; i++) {
    if (shm_table.shm_pages[i].id == id) {
      return shm_table.shm_pages + i;
    }
  }
  
  return 0;
}

int shm_open(int id, char **pointer) {

  struct shm_page * pg = 0;
  struct proc * currProc = myproc();
  uint i;
  char * va = (char *)PGROUNDUP(currProc->sz);

  acquire(&(shm_table.lock));

  pg = shm_get_page(id);

  if (pg) { //The page exists in shm_table
    //map page
    if (!mappages(currProc->pgdir, va, PGSIZE, V2P(pg->frame), PTE_W|PTE_U)) 
    {
      currProc->sz = (uint)va + PGSIZE;

      pg->refcnt++;

      *pointer = va;
    } else {
      cprintf("Error: Shared page with id %d could not be mapped.\n", id);
      release(&(shm_table.lock));
      return 1;
    }


  } else{ //page was not found in shm_table
    //create page
    for (i = 0; i < 64; i++) {
      if (!shm_table.shm_pages[i].frame) { //Find an open frame in shm_table
        pg = shm_table.shm_pages + i;
        break;
      }
    }

    if (!pg) {
      cprintf("Error: Shared memory table is full.\n");
      release(&(shm_table.lock));
      return 1;
    }

    pg->frame = kalloc();
    memset(pg->frame, 0, PGSIZE);

    if (!mappages(currProc->pgdir, va, PGSIZE, V2P(pg->frame), PTE_W|PTE_U)) 
    {
      currProc->sz = (uint)va + PGSIZE;
      *pointer = va;
      
      pg->id = id;
      pg->refcnt++; 
    } else {
      cprintf("Error: shared page with id %d could not be mapped.\n", id);
      release(&(shm_table.lock));
      return 1;
    }
    
  }
  
  release(&(shm_table.lock));

  return 0; 
}//All should be well


// Close a shared memory page for the current process.
// Return 1 if there was an error closing, 0 otherwise.
int shm_close(int id) {
  
  acquire(&(shm_table.lock));
  struct shm_page* pg = shm_get_page(id); 
  	
  if(pg == 0 || pg->refcnt == 0){
    cprintf("Error: No shared memory to close.\n\n");
    release(&(shm_table.lock));
    return 1; //Return error
  }
  else{
    pg->refcnt--;
  }
  
  if(pg->refcnt == 0){
    pg->id = 0;
    pg->frame = 0; 
  }
  release(&(shm_table.lock));

  return 0; 
}

