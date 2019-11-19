// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRAZY_LINKER_ELF_LOADER_H
#define CRAZY_LINKER_ELF_LOADER_H

#include "crazy_linker_error.h"
#include "crazy_linker_load_params.h"
#include "crazy_linker_memory_mapping.h"
#include "crazy_linker_system.h"  // For ScopedFileDescriptor
#include "elf_traits.h"

namespace crazy {

// Helper class used to load an ELF binary in memory.
//
// Note that this doesn't not perform any relocation, the purpose
// of this class is strictly to map all loadable segments from the
// file to their correct location.
//
class ElfLoader {
 public:
  // Result of the LoadAt method. In case of failure, an invalid instance
  // will be returned.
  struct Result {
    ELF::Addr load_start = 0;
    ELF::Addr load_size = 0;
    ELF::Addr load_bias = 0;
    const ELF::Phdr* phdr = nullptr;
    size_t phdr_count = 0;
    MemoryMapping reserved_mapping;

    constexpr bool IsValid() const { return this->load_start != 0; }
  };

  // Try to load a library at a given address. On failure, return an
  // invalid Result instance, and sets |*error|.
  static Result LoadAt(const LoadParams& params, Error* error);
};

}  // namespace crazy

#endif  // CRAZY_LINKER_ELF_LOADER_H
