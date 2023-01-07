/* Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */


/* XRay -- a simple profiler for Native Client */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "xray/xray_priv.h"

#if defined(XRAY)

struct XRaySymbol* XRaySymbolTableCreateEntry(struct XRaySymbolTable* symtab,
                                              char* line) {
  uint32_t addr;
  unsigned int uiaddr;
  char symbol_text[XRAY_LINE_SIZE];
  char* parsed_symbol;
  char* newln;
  if (2 != sscanf(line, "%x %1023s", &uiaddr, symbol_text))
    return NULL;
  if (uiaddr > 0x07FFFFFF) {
    fprintf(stderr, "While parsing the mapfile, XRay encountered:\n");
    fprintf(stderr, "%s\n", line);
    fprintf(stderr,
        "XRay only works with code addresses 0x00000000 - 0x07FFFFFF\n");
    fprintf(stderr, "All functions must reside in this address space.\n");
    exit(-1);
  }
  addr = (uint32_t)uiaddr;
  parsed_symbol = strstr(line, symbol_text);
  newln = strstr(parsed_symbol, "\n");
  if (NULL != newln) {
    *newln = 0;
  }
  return XRaySymbolTableAddByName(symtab, parsed_symbol, addr);
}


void XRaySymbolTableParseMapfile(struct XRaySymbolTable* symtab,
                                 const char* mapfile) {
  FILE* f;
  char line[XRAY_LINE_SIZE];
  bool in_text = false;
  bool in_link_once = false;
  int in_link_once_counter = 0;
  int num_symbols = 0;

  printf("XRay: opening mapfile %s\n", mapfile);
  f = fopen(mapfile, "rt");
  if (0 == f) {
    fprintf(stderr, "XRay: failed to open %s\n", mapfile);
    return;
  }
  printf("XRay: parsing...\n");
  while (NULL != fgets(line, XRAY_LINE_SIZE, f)) {
    if (line == strstr(line, " .text ")) {
      in_text = true;
      continue;
    }
    if (line == strstr(line, " .gnu.linkonce.t.")) {
      in_link_once = true;
      in_link_once_counter = 0;
      continue;
    }
    if (line == strstr(line, " .text.")) {
      in_link_once = true;
      in_link_once_counter = 0;
      continue;
    }
    if (line == strstr(line, "                0x")) {
      if (in_text) {
        XRaySymbolTableCreateEntry(symtab, line);
        ++num_symbols;
      } else if (in_link_once) {
        if (in_link_once_counter != 0) {
          if (NULL != XRaySymbolTableCreateEntry(symtab, line))
            ++num_symbols;
        } else {
          ++in_link_once_counter;
        }
      }
    } else {
      in_text = false;
      in_link_once = false;
    }
  }
  fclose(f);
  printf("XRay: parsed %d symbols into symbol table\n", num_symbols);
}

#endif  // XRAY
