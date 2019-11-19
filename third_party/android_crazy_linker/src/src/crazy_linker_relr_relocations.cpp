// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crazy_linker_relr_relocations.h"

#include <type_traits>

namespace crazy {

// Apply a single RELR relocation at virtual |offset| address, using
// the |load_bias| value.
static void ApplyRelrRelocation(ELF::Addr offset, size_t load_bias) {
  offset += load_bias;
  *reinterpret_cast<ELF::Addr*>(offset) += load_bias;
}

void RelrRelocations::Apply(size_t load_bias) {
  // Simple sanity checks.
  if (!relocations_ || !relocations_size_)
    return;

  const ELF::Relr* begin = relocations_;
  const ELF::Relr* end = begin + (relocations_size_ / sizeof(ELF::Relr));
  const size_t word_size = sizeof(ELF::Addr);

  ELF::Addr base = 0;  // current relocation address
  while (begin < end) {
    ELF::Relr entry = *begin++;

    if ((entry & 1) == 0) {
      // An even value corresponds to the address of the next relocation.
      ELF::Addr offset = static_cast<ELF::Addr>(entry);
      ApplyRelrRelocation(offset, load_bias);
      base = offset + word_size;
    } else {
      // An odd value corresponds to a bitmap of 31 or 63 words, based
      // on the CPU bitness / word_size.
      ELF::Addr offset = base;

      // Right shift of signed integers has undefined behaviour before C++20.
      static_assert(
          std::is_unsigned<decltype(entry)>::value,
          "The ELF::Relr type should be unsigned to avoid undefined behaviour");

      while (entry != 0) {
        entry >>= 1;
        if ((entry & 1) != 0)
          ApplyRelrRelocation(offset, load_bias);
        offset += word_size;
      }
      // Increment |base| by 31 or 63 words.
      base += (8 * word_size - 1) * word_size;
    }
  }
}

}  // namespace crazy
