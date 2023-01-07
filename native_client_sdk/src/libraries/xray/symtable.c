/* Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

/* XRay symbol table */

#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__GLIBC__)
#include <dlfcn.h>
#endif

#include "xray/xray_priv.h"
#define PNACL_STRING_OFFSET (0x10000000)

#if defined(XRAY)

bool g_symtable_debug = false;

struct XRayFrameInfo {
  int times_called;
  int total_ticks;
};


struct XRaySymbol {
  const char* name;
  struct XRayFrameInfo frames[XRAY_MAX_FRAMES];
};


struct XRaySymbolPoolNode {
  struct XRaySymbolPoolNode* next;
  struct XRaySymbol symbols[XRAY_SYMBOL_POOL_NODE_SIZE];
};


struct XRaySymbolPool {
  struct XRaySymbolPoolNode* head;
  struct XRaySymbolPoolNode* current;
  int index;
};


struct XRaySymbolTable {
  int num_symbols;
  struct XRayHashTable* hash_table;
  struct XRayStringPool* string_pool;
  struct XRaySymbolPool* symbol_pool;
};


const char* XRaySymbolGetName(struct XRaySymbol* symbol) {
  return (NULL == symbol) ? "(null)" : symbol->name;
}


struct XRaySymbol* XRaySymbolCreate(struct XRaySymbolPool* sympool,
                                    const char* name)
{
  struct XRaySymbol* symbol;
  symbol = XRaySymbolPoolAlloc(sympool);
  symbol->name = name;
  return symbol;
}


struct XRaySymbol* XRaySymbolPoolAlloc(struct XRaySymbolPool* sympool) {
  struct XRaySymbol* symbol;
  if (sympool->index >= XRAY_SYMBOL_POOL_NODE_SIZE) {
    struct XRaySymbolPoolNode* new_pool;
    new_pool = (struct XRaySymbolPoolNode*)XRayMalloc(sizeof(*new_pool));
    sympool->current->next = new_pool;
    sympool->current = new_pool;
    sympool->index = 0;
  }
  symbol = &sympool->current->symbols[sympool->index];
  ++sympool->index;
  return symbol;
}


struct XRaySymbolPool* XRaySymbolPoolCreate() {
  struct XRaySymbolPool* sympool;
  struct XRaySymbolPoolNode* node;
  sympool = (struct XRaySymbolPool*)XRayMalloc(sizeof(*sympool));
  node = (struct XRaySymbolPoolNode*)XRayMalloc(sizeof(*node));
  sympool->head = node;
  sympool->current = node;
  sympool->index = 0;
  return sympool;
}


void XRaySymbolPoolFree(struct XRaySymbolPool* pool) {
  struct XRaySymbolPoolNode* n = pool->head;
  while (NULL != n) {
    struct XRaySymbolPoolNode* c = n;
    n = n->next;
    XRayFree(c);
  }
  XRayFree(pool);
}


int XRaySymbolTableGetCount(struct XRaySymbolTable* symtab) {
  return XRayHashTableGetCount(symtab->hash_table);
}


struct XRaySymbol* XRaySymbolTableAtIndex(struct XRaySymbolTable* symtab,
                                          int i) {
  return (struct XRaySymbol*)XRayHashTableAtIndex(symtab->hash_table, i);
}

struct XRaySymbol* XRaySymbolTableAdd(struct XRaySymbolTable* symtab,
                                      struct XRaySymbol* symbol,
                                      uint32_t addr) {
  struct XRaySymbol* sym = (struct XRaySymbol*)
      XRayHashTableInsert(symtab->hash_table, symbol, addr);
  symtab->num_symbols = XRayHashTableGetCount(symtab->hash_table);
  return sym;
}

struct XRaySymbol* XRaySymbolTableAddByName(struct XRaySymbolTable* symtab,
                                            const char* name, uint32_t addr) {
  char* recorded_name;
  struct XRaySymbol* symbol;
  char buffer[XRAY_LINE_SIZE];
  const char* demangled_name = XRayDemangle(buffer, XRAY_LINE_SIZE, name);
  /* record the demangled symbol name into the string pool */
  recorded_name = XRayStringPoolAppend(symtab->string_pool, demangled_name);
  if (g_symtable_debug)
    printf("adding symbol %s\n", recorded_name);
  /* construct a symbol and put it in the symbol table */
  symbol = XRaySymbolCreate(symtab->symbol_pool, recorded_name);
  return XRaySymbolTableAdd(symtab, symbol, addr);
}

struct XRaySymbol* XRaySymbolTableLookup(struct XRaySymbolTable* symtab,
                                         uint32_t addr) {
  void *x = XRayHashTableLookup(symtab->hash_table, addr);
  struct XRaySymbol* r = (struct XRaySymbol*)x;

#if defined(__pnacl__)
  if (r == NULL) {
    /* Addresses are trimed to 24 bits for internal storage, so we need to
     * add this offset back in order to get the real address.
     */
    addr |= PNACL_STRING_OFFSET;
    const char* name = (const char*)addr;
    struct XRaySymbol* symbol = XRaySymbolCreate(symtab->symbol_pool, name);
    r = XRaySymbolTableAdd(symtab, symbol, addr);
  }
#endif

#if defined(__GLIBC__)
  if (r == NULL) {
    Dl_info info;
    if (dladdr((const void*)addr, &info) != 0)
      if (info.dli_sname)
        r = XRaySymbolTableAddByName(symtab, info.dli_sname, addr);
  }
#endif
  return r;
}


/* Returns total number of symbols in the table. */
int XRaySymbolCount(struct XRaySymbolTable* symtab) {
  return symtab->num_symbols;
}


/* Creates and inializes a symbol table. */
struct XRaySymbolTable* XRaySymbolTableCreate(int size) {
  struct XRaySymbolTable* symtab;
  symtab = (struct XRaySymbolTable*)XRayMalloc(sizeof(*symtab));
  symtab->num_symbols = 0;
  symtab->string_pool = XRayStringPoolCreate();
  symtab->hash_table = XRayHashTableCreate(size);
  symtab->symbol_pool = XRaySymbolPoolCreate();
  return symtab;
}


/* Frees a symbol table. */
void XRaySymbolTableFree(struct XRaySymbolTable* symtab) {
  XRayStringPoolFree(symtab->string_pool);
  XRaySymbolPoolFree(symtab->symbol_pool);
  XRayHashTableFree(symtab->hash_table);
  symtab->num_symbols = 0;
  XRayFree(symtab);
}

#endif  /* XRAY */
