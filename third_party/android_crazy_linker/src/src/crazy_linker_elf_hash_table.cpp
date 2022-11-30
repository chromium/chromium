// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crazy_linker_elf_hash_table.h"

#include <string.h>

namespace crazy {

// Compute the ELF hash of a given symbol.
static unsigned ElfHash(const char* name) {
  const uint8_t* ptr = reinterpret_cast<const uint8_t*>(name);
  unsigned h = 0;
  while (*ptr) {
    h = (h << 4) + *ptr++;
    unsigned g = h & 0xf0000000;
    h ^= g;
    h ^= g >> 24;
  }
  return h;
}

void ElfHashTable::Init(uintptr_t dt_elf_hash) {
  const ELF::Word* data = reinterpret_cast<const ELF::Word*>(dt_elf_hash);
  hash_bucket_size_ = data[0];
  hash_bucket_ = data + 2;
  hash_chain_size_ = data[1];
  hash_chain_ = hash_bucket_ + hash_bucket_size_;
}

bool ElfHashTable::IsValid() const {
  return hash_bucket_size_ > 0;
}

const ELF::Sym* ElfHashTable::LookupByName(const char* symbol_name,
                                           const ELF::Sym* symbol_table,
                                           const char* string_table) const {
  unsigned hash = ElfHash(symbol_name);

  for (unsigned n = hash_bucket_[hash % hash_bucket_size_]; n != 0;
       n = hash_chain_[n]) {
    const ELF::Sym* symbol = &symbol_table[n];
    // Check that the symbol has the appropriate name.
    if (!strcmp(string_table + symbol->st_name, symbol_name))
      return symbol;
  }
  return nullptr;
}

}  // namespace crazy
