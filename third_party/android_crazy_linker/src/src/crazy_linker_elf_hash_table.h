// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRAZY_LINKER_ELF_HASH_TABLE_H
#define CRAZY_LINKER_ELF_HASH_TABLE_H

#include <stddef.h>
#include "elf_traits.h"

namespace crazy {

// Models the hash table used to map symbol names to symbol entries using
// the standard ELF format.
class ElfHashTable {
 public:
  // Initialize instance. |dt_elf_hash| should be the address that the
  // DT_HASH entry points to in the input ELF dynamic section. Call IsValid()
  // to determine whether the table was well-formed.
  void Init(uintptr_t dt_elf_hash);

  // Returns true iff the content of the table is valid.
  bool IsValid() const;

  // Index of the first dynamic symbol within the ELF symbol table.
  size_t dyn_symbols_offset() const { return 1; }

  // Number of dynamic symbols in the ELF symbol table.
  size_t dyn_symbols_count() const { return hash_chain_size_ - 1; }

  // Lookup |symbol_name| in the table. |symbol_table| should point to the
  // ELF symbol table, and |string_table| to the start of its string table.
  // Returns nullptr on failure.
  const ELF::Sym* LookupByName(const char* symbol_name,
                               const ELF::Sym* symbol_table,
                               const char* string_table) const;

 private:
  const ELF::Word* hash_bucket_ = nullptr;
  size_t hash_bucket_size_ = 0;
  const ELF::Word* hash_chain_ = nullptr;
  size_t hash_chain_size_ = 0;
};

}  // namespace crazy

#endif  // CRAZY_LINKER_ELF_HASH_TABLE_H
