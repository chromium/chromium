// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crazy_linker_gnu_hash_table.h"

#include <string.h>

namespace crazy {

// Compute the GNU hash of a given symbol.
static uint32_t GnuHash(const char* name) {
  uint32_t h = 5381;
  const uint8_t* ptr = reinterpret_cast<const uint8_t*>(name);
  while (*ptr) {
    h = h * 33 + *ptr++;
  }
  return h;
}

void GnuHashTable::Init(uintptr_t dt_gnu_hash) {
  sym_count_ = 0;

  const uint32_t* data = reinterpret_cast<const uint32_t*>(dt_gnu_hash);
  num_buckets_ = data[0];
  sym_offset_ = data[1];

  if (!num_buckets_)
    return;

  const uint32_t bloom_size = data[2];
  if ((bloom_size & (bloom_size - 1U)) != 0)  // must be a power of 2
    return;

  bloom_word_mask_ = bloom_size - 1U;
  bloom_shift_ = data[3];
  bloom_filter_ = reinterpret_cast<const ELF::Addr*>(data + 4);
  buckets_ = reinterpret_cast<const uint32_t*>(bloom_filter_ + bloom_size);
  chain_ = buckets_ + num_buckets_;

  // Compute number of dynamic symbols by parsing the table.
  if (num_buckets_ > 0) {
    // First find the maximum index in the buckets table.
    uint32_t max_index = buckets_[0];
    for (size_t n = 1; n < num_buckets_; ++n) {
      uint32_t sym_index = buckets_[n];
      if (sym_index > max_index)
        max_index = sym_index;
    }
    // Now start to look at the chain_ table from (max_index - sym_offset_)
    // until there is a value with LSB set to 1, indicating the end of the
    // last chain.
    while ((chain_[max_index - sym_offset_] & 1) == 0)
      max_index++;

    sym_count_ = (max_index - sym_offset_) + 1;
  }
}

bool GnuHashTable::IsValid() const {
  return sym_count_ > 0;
}

const ELF::Sym* GnuHashTable::LookupByName(const char* symbol_name,
                                           const ELF::Sym* symbol_table,
                                           const char* string_table) const {
  uint32_t hash = GnuHash(symbol_name);

  // First, bloom filter test.
  const unsigned kElfBits = ELF::kElfBits;
  ELF::Addr word = bloom_filter_[(hash / kElfBits) & bloom_word_mask_];
  ELF::Addr mask = (ELF::Addr(1) << (hash % kElfBits)) |
                   (ELF::Addr(1) << ((hash >> bloom_shift_) % kElfBits));

  if ((word & mask) != mask)
    return nullptr;

  uint32_t sym_index = buckets_[hash % num_buckets_];
  if (sym_index < sym_offset_)
    return nullptr;

  while (true) {
    const ELF::Sym* sym = symbol_table + sym_index;
    const uint32_t sym_hash = chain_[sym_index - sym_offset_];
    const char* sym_name = string_table + sym->st_name;

    if ((sym_hash | 1) == (hash | 1) && !strcmp(sym_name, symbol_name)) {
      return sym;
    }

    if (sym_hash & 1)
      break;

    sym_index++;
  }

  return nullptr;
}

}  // namespace crazy
