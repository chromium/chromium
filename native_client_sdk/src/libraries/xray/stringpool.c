/* Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */


/* XRay string pool */

/* String pool holds a large pile of strings. */
/* It is up to higher level data structures to avoid duplicates. */
/* It is up to higher level data structures to provide fast lookups. */

/* _GNU_SOURCE must be defined prior to the inclusion of string.h
 * so that strnlen is available with glibc */
#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include "xray/xray_priv.h"

#if defined(XRAY)

struct XRayStringPoolNode {
  struct XRayStringPoolNode* next;
  char strings[XRAY_STRING_POOL_NODE_SIZE];
};


struct XRayStringPool {
  struct XRayStringPoolNode* head;
  struct XRayStringPoolNode* current;
  int index;
};


static struct XRayStringPoolNode* XRayStringPoolAllocNode() {
  struct XRayStringPoolNode* s;
  s = (struct XRayStringPoolNode *)XRayMalloc(sizeof(*s));
  s->next = NULL;
  return s;
}


static int XRayStringPoolCurrentNodeSpaceAvail(struct XRayStringPool* pool) {
  int i = pool->index;
  return (XRAY_STRING_POOL_NODE_SIZE - i) - 1;
}


/* Append a string to the string pool. */
char* XRayStringPoolAppend(struct XRayStringPool* pool, const char* src) {
  /* Add +1 to STRING_POOL_NODE_SIZE to detect large strings */
  /* Add +1 to strnlen result to account for string termination */
  int n = strnlen(src, XRAY_STRING_POOL_NODE_SIZE + 1) + 1;
  int a = XRayStringPoolCurrentNodeSpaceAvail(pool);
  char* dst;
  /* Don't accept strings larger than the pool node. */
  if (n >= (XRAY_STRING_POOL_NODE_SIZE - 1))
    return NULL;
  /* If string doesn't fit, alloc a new node. */
  if (n > a) {
    pool->current->next = XRayStringPoolAllocNode();
    pool->current = pool->current->next;
    pool->index = 0;
  }
  /* Copy string and return a pointer to copy. */
  dst = &pool->current->strings[pool->index];
  strcpy(dst, src);
  pool->index += n;
  return dst;
}


/* Create & initialize a string pool instance. */
struct XRayStringPool* XRayStringPoolCreate() {
  struct XRayStringPool* pool;
  pool = (struct XRayStringPool*)XRayMalloc(sizeof(*pool));
  pool->head = XRayStringPoolAllocNode();
  pool->current = pool->head;
  return pool;
}


/* Free a string pool. */
void XRayStringPoolFree(struct XRayStringPool* pool) {
  struct XRayStringPoolNode* n = pool->head;
  while (NULL != n) {
    struct XRayStringPoolNode* c = n;
    n = n->next;
    XRayFree(c);
  }
  XRayFree(pool);
}

#endif  /* XRAY */

