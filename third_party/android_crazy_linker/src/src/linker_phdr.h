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
#ifndef LINKER_PHDR_H
#define LINKER_PHDR_H

/* Declarations related to the ELF program header table and segments.
 *
 * The design goal is to provide an API that is as close as possible
 * to the ELF spec, and does not depend on linker-specific data
 * structures (e.g. the exact layout of struct soinfo).
 */

#include "elf_traits.h"

size_t phdr_table_get_load_size(const ELF::Phdr* phdr_table,
                                size_t phdr_count,
                                ELF::Addr* min_vaddr = NULL,
                                ELF::Addr* max_vaddr = NULL);

int phdr_table_protect_segments(const ELF::Phdr* phdr_table,
                                int phdr_count,
                                ELF::Addr load_bias);

int phdr_table_unprotect_segments(const ELF::Phdr* phdr_table,
                                  int phdr_count,
                                  ELF::Addr load_bias);

int phdr_table_get_relro_info(const ELF::Phdr* phdr_table,
                              int phdr_count,
                              ELF::Addr load_bias,
                              ELF::Addr* relro_start,
                              ELF::Addr* relro_size);

int phdr_table_protect_gnu_relro(const ELF::Phdr* phdr_table,
                                 int phdr_count,
                                 ELF::Addr load_bias);

#ifdef __arm__
int phdr_table_get_arm_exidx(const ELF::Phdr* phdr_table,
                             int phdr_count,
                             ELF::Addr load_bias,
                             ELF::Addr** arm_exidx,
                             unsigned* arm_exidix_count);
#endif

void phdr_table_get_dynamic_section(const ELF::Phdr* phdr_table,
                                    int phdr_count,
                                    ELF::Addr load_bias,
                                    const ELF::Dyn** dynamic,
                                    size_t* dynamic_count,
                                    ELF::Word* dynamic_flags);

#endif /* LINKER_PHDR_H */
