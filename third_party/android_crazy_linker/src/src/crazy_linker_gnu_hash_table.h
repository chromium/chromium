// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRAZY_LINKER_GNU_HASH_TABLE_H
#define CRAZY_LINKER_GNU_HASH_TABLE_H

#include <stddef.h>
#include "elf_traits.h"

namespace crazy {

// Models the hash table used to map symbol names to symbol entries using
// the GNU format. This one is smaller and faster than the standard ELF one.
class GnuHashTable {
 public:
  // Initialize instance. |dt_gnu_hash| should be the address that the
  // DT_GNU_HASH entry points to in the input ELF dynamic section. Call
  // IsValid() to determine whether the table was well-formed.
  void Init(uintptr_t dt_gnu_hash);

  // Returns true iff the content of the table is valid.
  bool IsValid() const;

  // Return the index of the first dynamic symbol within the ELF symbol table.
  size_t dyn_symbols_offset() const { return sym_offset_; };

  // Number of dynamic symbols in the ELF symbol table.
  size_t dyn_symbols_count() const { return sym_count_; }

  // Lookup |symbol_name| in the table. |symbol_table| should point to the
  // ELF symbol table, and |string_table| to the start of its string table.
  // Returns nullptr on failure.
  const ELF::Sym* LookupByName(const char* symbol_name,
                               const ELF::Sym* symbol_table,
                               const char* string_table) const;

 private:
  uint32_t num_buckets_ = 0;
  uint32_t sym_offset_ = 0;
  uint32_t sym_count_ = 0;
  uint32_t bloom_word_mask_ = 0;
  uint32_t bloom_shift_ = 0;
  const ELF::Addr* bloom_filter_ = nullptr;
  const uint32_t* buckets_ = nullptr;
  const uint32_t* chain_ = nullptr;
};

}  // namespace crazy

#endif  // CRAZY_LINKER_GNU_HASH_TABLE_H
