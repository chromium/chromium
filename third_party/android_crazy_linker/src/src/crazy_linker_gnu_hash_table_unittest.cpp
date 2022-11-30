// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crazy_linker_gnu_hash_table.h"

#include <gtest/gtest.h>
#include "crazy_linker_gnu_hash_table_test_data.h"

#include <stdint.h>

namespace crazy {
namespace testing {

TEST(GnuHashTable, LookupByName) {
  GnuHashTable table;
  table.Init(reinterpret_cast<uintptr_t>(&kTestGnuHashTable));
  EXPECT_TRUE(table.IsValid());

  const ELF::Sym* sym;

#define CHECK_SYMBOL(name, offset, address, size)                           \
  sym = table.LookupByName(name, kTestGnuSymbolTable, kTestGnuStringTable); \
  EXPECT_TRUE(sym) << name;                                                 \
  EXPECT_EQ((address), sym->st_value) << name;                              \
  EXPECT_EQ((size), sym->st_size) << name;

  LIST_ELF_SYMBOLS_TestGnu(CHECK_SYMBOL);

#undef CHECK_SYMBOL

  // Check a few undefined symbols.
  EXPECT_FALSE(table.LookupByName("ahahahahah", kTestGnuSymbolTable,
                                  kTestGnuStringTable));
  EXPECT_FALSE(
      table.LookupByName("strsign", kTestGnuSymbolTable, kTestGnuStringTable));
}

TEST(GnuHashTable, DynSymbols) {
  GnuHashTable table;
  table.Init(reinterpret_cast<uintptr_t>(&kTestGnuHashTable));
  EXPECT_TRUE(table.IsValid());

  const size_t kExpectedOffset = kTestGnuHashTable[1];
  const size_t kExpectedCount =
      (sizeof(kTestGnuSymbolTable) / sizeof(kTestGnuSymbolTable[0])) -
      kExpectedOffset;

  EXPECT_EQ(kExpectedOffset, table.dyn_symbols_offset());
  EXPECT_EQ(kExpectedCount, table.dyn_symbols_count());
}

}  // namespace testing
}  // namespace crazy
