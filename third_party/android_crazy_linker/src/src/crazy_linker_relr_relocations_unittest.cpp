// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crazy_linker_relr_relocations.h"

#include "elf_traits.h"

#include <gtest/gtest.h>

namespace crazy {

TEST(RelrRelocations, DefaultConstruction) {
  RelrRelocations relr;
  relr.Apply(0);
}

TEST(RelrRelocations, ApplyWithSimpleAddressList) {
  // Build a simple array of addresses initialized to pretty liberal values.
  const size_t kDataSize = 128;
  ELF::Addr data[kDataSize];
  for (size_t n = 0; n < kDataSize; ++n)
    data[n] = ELF::Addr(0) + n;

  // Build an RELR table that lists every *even* item in data[] as a RELR
  // relocation target. Offsets are relative to |data|, which thus must be
  // the load bias when calling RelrRelocations::Apply() below.
  const size_t kWordSize = sizeof(ELF::Addr);
  const size_t kRelrSize = 8;
  ELF::Relr relr_table[kRelrSize];
  for (size_t n = 0; n < kRelrSize; ++n) {
    relr_table[n] = kWordSize * n * 2;
  }
  RelrRelocations relr;
  relr.SetAddress(reinterpret_cast<uintptr_t>(relr_table));
  relr.SetSize(static_cast<ELF::Addr>(sizeof(relr_table)));

  // Build the expected data table here, it's easier to understand the
  // comparison.
  auto load_bias = reinterpret_cast<uintptr_t>(data);
  ELF::Addr expected[kDataSize];
  for (size_t n = 0; n < kDataSize; ++n) {
    if ((n < 2 * kRelrSize) && (n & 1) == 0) {
      expected[n] = data[n] + load_bias;
    } else {
      expected[n] = data[n];
    }
  }

  // Apply RELR relocations.
  relr.Apply(load_bias);

  // Compare results.
  for (size_t n = 0; n < kDataSize; ++n) {
    EXPECT_EQ(expected[n], data[n]) << "# " << n;
  }
}

TEST(RelrRelocations, ApplyWithSimpleBitmaps) {
  // Build a simple array of addresses initialized to pretty liberal values.
  const size_t kDataSize = 128;
  ELF::Addr data[kDataSize];
  for (size_t n = 0; n < kDataSize; ++n)
    data[n] = ELF::Addr(0) + n;

  // Build an RELR table that lists every 3rd item in data[] as a RELR
  // relocation target. Using only bitmaps. Base address is 0.
  const size_t kWordSize = sizeof(ELF::Addr);
  const size_t kBitsPerWord = (kWordSize * 8 - 1);
  const size_t kRelrSize = (kDataSize + kBitsPerWord - 1) / kBitsPerWord;
  ELF::Relr relr_table[kRelrSize] = {};
  for (size_t n = 0; n < kDataSize; ++n) {
    if ((n % 3) == 0)
      relr_table[n / kBitsPerWord] |= 1U | (ELF::Relr(2) << (n % kBitsPerWord));
  }
  RelrRelocations relr;
  relr.SetAddress(reinterpret_cast<uintptr_t>(relr_table));
  relr.SetSize(static_cast<ELF::Addr>(sizeof(relr_table)));

  // Build the expected data table here, it's easier to understand the
  // comparison.
  auto load_bias = reinterpret_cast<uintptr_t>(data);
  ELF::Addr expected[kDataSize];
  for (size_t n = 0; n < kDataSize; ++n) {
    if ((n % 3) == 0) {
      expected[n] = data[n] + load_bias;
    } else {
      expected[n] = data[n];
    }
  }

  // Apply RELR relocations.
  relr.Apply(load_bias);

  // Compare results.
  for (size_t n = 0; n < kDataSize; ++n) {
    EXPECT_EQ(expected[n], data[n]) << "# " << n;
  }
}

}  // namespace crazy
