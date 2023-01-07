// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
 * Copyright (C) 2012 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "linker_phdr.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#define PAGE_START(x) ((x) & PAGE_MASK)
#define PAGE_OFFSET(x) ((x) & ~PAGE_MASK)
#define PAGE_END(x) PAGE_START((x) + (PAGE_SIZE - 1))

// Missing exec_elf.h definitions.
#ifndef PT_GNU_RELRO
#define PT_GNU_RELRO 0x6474e552
#endif

/**
  TECHNICAL NOTE ON ELF LOADING.

  An ELF file's program header table contains one or more PT_LOAD
  segments, which corresponds to portions of the file that need to
  be mapped into the process' address space.

  Each loadable segment has the following important properties:

    p_offset  -> segment file offset
    p_filesz  -> segment file size
    p_memsz   -> segment memory size (always >= p_filesz)
    p_vaddr   -> segment's virtual address
    p_flags   -> segment flags (e.g. readable, writable, executable)

  We will ignore the p_paddr and p_align fields of ELF::Phdr for now.

  The loadable segments can be seen as a list of [p_vaddr ... p_vaddr+p_memsz)
  ranges of virtual addresses. A few rules apply:

  - the virtual address ranges should not overlap.

  - if a segment's p_filesz is smaller than its p_memsz, the extra bytes
    between them should always be initialized to 0.

  - ranges do not necessarily start or end at page boundaries. Two distinct
    segments can have their start and end on the same page. In this case, the
    page inherits the mapping flags of the latter segment.

  Finally, the real load addrs of each segment is not p_vaddr. Instead the
  loader decides where to load the first segment, then will load all others
  relative to the first one to respect the initial range layout.

  For example, consider the following list:

    [ offset:0,      filesz:0x4000, memsz:0x4000, vaddr:0x30000 ],
    [ offset:0x4000, filesz:0x2000, memsz:0x8000, vaddr:0x40000 ],

  This corresponds to two segments that cover these virtual address ranges:

       0x30000...0x34000
       0x40000...0x48000

  If the loader decides to load the first segment at address 0xa0000000
  then the segments' load address ranges will be:

       0xa0030000...0xa0034000
       0xa0040000...0xa0048000

  In other words, all segments must be loaded at an address that has the same
  constant offset from their p_vaddr value. This offset is computed as the
  difference between the first segment's load address, and its p_vaddr value.

  However, in practice, segments do _not_ start at page boundaries. Since we
  can only memory-map at page boundaries, this means that the bias is
  computed as:

       load_bias = phdr0_load_address - PAGE_START(phdr0->p_vaddr)

  (NOTE: The value must be used as a 32-bit unsigned integer, to deal with
          possible wrap around UINT32_MAX for possible large p_vaddr values).

  And that the phdr0_load_address must start at a page boundary, with
  the segment's real content starting at:

       phdr0_load_address + PAGE_OFFSET(phdr0->p_vaddr)

  Note that ELF requires the following condition to make the mmap()-ing work:

      PAGE_OFFSET(phdr0->p_vaddr) == PAGE_OFFSET(phdr0->p_offset)

  The load_bias must be added to any p_vaddr value read from the ELF file to
  determine the corresponding memory address.

 **/

#define MAYBE_MAP_FLAG(x, from, to) (((x) & (from)) ? (to) : 0)
#define PFLAGS_TO_PROT(x)                 \
  (MAYBE_MAP_FLAG((x), PF_X, PROT_EXEC) | \
   MAYBE_MAP_FLAG((x), PF_R, PROT_READ) | \
   MAYBE_MAP_FLAG((x), PF_W, PROT_WRITE))

/* Returns the size of the extent of all the possibly non-contiguous
 * loadable segments in an ELF program header table. This corresponds
 * to the page-aligned size in bytes that needs to be reserved in the
 * process' address space. If there are no loadable segments, 0 is
 * returned.
 *
 * If out_min_vaddr or out_max_vaddr are non-NULL, they will be
 * set to the minimum and maximum addresses of pages to be reserved,
 * or 0 if there is nothing to load.
 */
size_t phdr_table_get_load_size(const ELF::Phdr* phdr_table,
                                size_t phdr_count,
                                ELF::Addr* out_min_vaddr,
                                ELF::Addr* out_max_vaddr) {
  ELF::Addr min_vaddr = ~static_cast<ELF::Addr>(0);
  ELF::Addr max_vaddr = 0x00000000U;

  bool found_pt_load = false;
  for (size_t i = 0; i < phdr_count; ++i) {
    const ELF::Phdr* phdr = &phdr_table[i];

    if (phdr->p_type != PT_LOAD) {
      continue;
    }
    found_pt_load = true;

    if (phdr->p_vaddr < min_vaddr) {
      min_vaddr = phdr->p_vaddr;
    }

    if (phdr->p_vaddr + phdr->p_memsz > max_vaddr) {
      max_vaddr = phdr->p_vaddr + phdr->p_memsz;
    }
  }
  if (!found_pt_load) {
    min_vaddr = 0x00000000U;
  }

  min_vaddr = PAGE_START(min_vaddr);
  max_vaddr = PAGE_END(max_vaddr);

  if (out_min_vaddr != NULL) {
    *out_min_vaddr = min_vaddr;
  }
  if (out_max_vaddr != NULL) {
    *out_max_vaddr = max_vaddr;
  }
  return max_vaddr - min_vaddr;
}

/* Used internally. Used to set the protection bits of all loaded segments
 * with optional extra flags (i.e. really PROT_WRITE). Used by
 * phdr_table_protect_segments and phdr_table_unprotect_segments.
 */
static int _phdr_table_set_load_prot(const ELF::Phdr* phdr_table,
                                     int phdr_count,
                                     ELF::Addr load_bias,
                                     int extra_prot_flags) {
  const ELF::Phdr* phdr = phdr_table;
  const ELF::Phdr* phdr_limit = phdr + phdr_count;

  for (; phdr < phdr_limit; phdr++) {
    if (phdr->p_type != PT_LOAD || (phdr->p_flags & PF_W) != 0)
      continue;

    ELF::Addr seg_page_start = PAGE_START(phdr->p_vaddr) + load_bias;
    ELF::Addr seg_page_end =
        PAGE_END(phdr->p_vaddr + phdr->p_memsz) + load_bias;

    int ret = mprotect((void*)seg_page_start,
                       seg_page_end - seg_page_start,
                       PFLAGS_TO_PROT(phdr->p_flags) | extra_prot_flags);
    if (ret < 0) {
      return -1;
    }
  }
  return 0;
}

/* Restore the original protection modes for all loadable segments.
 * You should only call this after phdr_table_unprotect_segments and
 * applying all relocations.
 *
 * Input:
 *   phdr_table  -> program header table
 *   phdr_count  -> number of entries in tables
 *   load_bias   -> load bias
 * Return:
 *   0 on error, -1 on failure (error code in errno).
 */
int phdr_table_protect_segments(const ELF::Phdr* phdr_table,
                                int phdr_count,
                                ELF::Addr load_bias) {
  return _phdr_table_set_load_prot(phdr_table, phdr_count, load_bias, 0);
}

/* Change the protection of all loaded segments in memory to writable.
 * This is useful before performing relocations. Once completed, you
 * will have to call phdr_table_protect_segments to restore the original
 * protection flags on all segments.
 *
 * Note that some writable segments can also have their content turned
 * to read-only by calling phdr_table_protect_gnu_relro. This is no
 * performed here.
 *
 * Input:
 *   phdr_table  -> program header table
 *   phdr_count  -> number of entries in tables
 *   load_bias   -> load bias
 * Return:
 *   0 on error, -1 on failure (error code in errno).
 */
int phdr_table_unprotect_segments(const ELF::Phdr* phdr_table,
                                  int phdr_count,
                                  ELF::Addr load_bias) {
  return _phdr_table_set_load_prot(
      phdr_table, phdr_count, load_bias, PROT_WRITE);
}

/* Return the extend of the GNU RELRO segment in a program header.
 * On success, return 0 and sets |*relro_start| and |*relro_end|
 * to the page-aligned extents of the RELRO section.
 * On failure, return -1.
 *
 * NOTE: This assumes there is a single PT_GNU_RELRO segment in the
 * program header, i.e. it will return the extents of the first entry.
 */
int phdr_table_get_relro_info(const ELF::Phdr* phdr_table,
                              int phdr_count,
                              ELF::Addr load_bias,
                              ELF::Addr* relro_start,
                              ELF::Addr* relro_size) {
  const ELF::Phdr* phdr;
  const ELF::Phdr* phdr_limit = phdr_table + phdr_count;

  for (phdr = phdr_table; phdr < phdr_limit; ++phdr) {
    if (phdr->p_type != PT_GNU_RELRO)
      continue;

    /* Tricky: what happens when the relro segment does not start
     * or end at page boundaries?. We're going to be over-protective
     * here and put every page touched by the segment as read-only.
     *
     * This seems to match Ian Lance Taylor's description of the
     * feature at http://www.airs.com/blog/archives/189.
     *
     * Extract:
     *    Note that the current dynamic linker code will only work
     *    correctly if the PT_GNU_RELRO segment starts on a page
     *    boundary. This is because the dynamic linker rounds the
     *    p_vaddr field down to the previous page boundary. If
     *    there is anything on the page which should not be read-only,
     *    the program is likely to fail at runtime. So in effect the
     *    linker must only emit a PT_GNU_RELRO segment if it ensures
     *    that it starts on a page boundary.
     */
    *relro_start = PAGE_START(phdr->p_vaddr) + load_bias;
    *relro_size =
        PAGE_END(phdr->p_vaddr + phdr->p_memsz) + load_bias - *relro_start;
    return 0;
  }

  return -1;
}

/* Apply GNU relro protection if specified by the program header. This will
 * turn some of the pages of a writable PT_LOAD segment to read-only, as
 * specified by one or more PT_GNU_RELRO segments. This must be always
 * performed after relocations.
 *
 * The areas typically covered are .got and .data.rel.ro, these are
 * read-only from the program's POV, but contain absolute addresses
 * that need to be relocated before use.
 *
 * Input:
 *   phdr_table  -> program header table
 *   phdr_count  -> number of entries in tables
 *   load_bias   -> load bias
 * Return:
 *   0 on error, -1 on failure (error code in errno).
 */
int phdr_table_protect_gnu_relro(const ELF::Phdr* phdr_table,
                                 int phdr_count,
                                 ELF::Addr load_bias) {
  ELF::Addr relro_start, relro_size;

  if (phdr_table_get_relro_info(
          phdr_table, phdr_count, load_bias, &relro_start, &relro_size) < 0) {
    return -1;
  }

  return mprotect((void*)relro_start, relro_size, PROT_READ);
}

#ifdef __arm__

#ifndef PT_ARM_EXIDX
#define PT_ARM_EXIDX 0x70000001 /* .ARM.exidx segment */
#endif

/* Return the address and size of the .ARM.exidx section in memory,
 * if present.
 *
 * Input:
 *   phdr_table  -> program header table
 *   phdr_count  -> number of entries in tables
 *   load_bias   -> load bias
 * Output:
 *   arm_exidx       -> address of table in memory (NULL on failure).
 *   arm_exidx_count -> number of items in table (0 on failure).
 * Return:
 *   0 on error, -1 on failure (_no_ error code in errno)
 */
int phdr_table_get_arm_exidx(const ELF::Phdr* phdr_table,
                             int phdr_count,
                             ELF::Addr load_bias,
                             ELF::Addr** arm_exidx,
                             unsigned* arm_exidx_count) {
  const ELF::Phdr* phdr = phdr_table;
  const ELF::Phdr* phdr_limit = phdr + phdr_count;

  for (phdr = phdr_table; phdr < phdr_limit; phdr++) {
    if (phdr->p_type != PT_ARM_EXIDX)
      continue;

    *arm_exidx = (ELF::Addr*)(load_bias + phdr->p_vaddr);
    *arm_exidx_count = (unsigned)(phdr->p_memsz / 8);
    return 0;
  }
  *arm_exidx = NULL;
  *arm_exidx_count = 0;
  return -1;
}
#endif  // __arm__

/* Return the address and size of the ELF file's .dynamic section in memory,
 * or NULL if missing.
 *
 * Input:
 *   phdr_table  -> program header table
 *   phdr_count  -> number of entries in tables
 *   load_bias   -> load bias
 * Output:
 *   dynamic       -> address of table in memory (NULL on failure).
 *   dynamic_count -> number of items in table (0 on failure).
 *   dynamic_flags -> protection flags for section (unset on failure)
 * Return:
 *   void
 */
void phdr_table_get_dynamic_section(const ELF::Phdr* phdr_table,
                                    int phdr_count,
                                    ELF::Addr load_bias,
                                    const ELF::Dyn** dynamic,
                                    size_t* dynamic_count,
                                    ELF::Word* dynamic_flags) {
  const ELF::Phdr* phdr = phdr_table;
  const ELF::Phdr* phdr_limit = phdr + phdr_count;

  for (phdr = phdr_table; phdr < phdr_limit; phdr++) {
    if (phdr->p_type != PT_DYNAMIC) {
      continue;
    }

    *dynamic = reinterpret_cast<const ELF::Dyn*>(load_bias + phdr->p_vaddr);
    if (dynamic_count) {
      *dynamic_count = (unsigned)(phdr->p_memsz / sizeof(ELF::Dyn));
    }
    if (dynamic_flags) {
      *dynamic_flags = phdr->p_flags;
    }
    return;
  }
  *dynamic = NULL;
  if (dynamic_count) {
    *dynamic_count = 0;
  }
}
