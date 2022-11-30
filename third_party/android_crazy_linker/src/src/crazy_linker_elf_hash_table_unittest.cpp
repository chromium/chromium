// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crazy_linker_elf_hash_table.h"

#include <gtest/gtest.h>
#include "crazy_linker_elf_hash_table_test_data.h"

#include <stdint.h>

namespace crazy {
namespace testing {

TEST(ElfHashTable, LookupByName) {
  ElfHashTable table;
  table.Init(reinterpret_cast<uintptr_t>(&kTestElfHashTable));
  EXPECT_TRUE(table.IsValid());

  const ELF::Sym* sym;

#define CHECK_SYMBOL(name, offset, address, size)                           \
  sym = table.LookupByName(name, kTestElfSymbolTable, kTestElfStringTable); \
  EXPECT_TRUE(sym) << name;                                                 \
  EXPECT_EQ((address), sym->st_value) << name;                              \
  EXPECT_EQ((size), sym->st_size) << name;

  LIST_ELF_SYMBOLS_TestElf(CHECK_SYMBOL);

#undef CHECK_SYMBOL

  // Check a few undefined symbols.
  EXPECT_FALSE(table.LookupByName("ahahahahah", kTestElfSymbolTable,
                                  kTestElfStringTable));
  EXPECT_FALSE(
      table.LookupByName("strsign", kTestElfSymbolTable, kTestElfStringTable));
}

TEST(ElfHashTable, DynSymbols) {
  ElfHashTable table;
  table.Init(reinterpret_cast<uintptr_t>(&kTestElfHashTable));
  EXPECT_TRUE(table.IsValid());

  const size_t kExpectedOffset = 1U;
  const size_t kExpectedCount =
      (sizeof(kTestElfSymbolTable) / sizeof(kTestElfSymbolTable[0])) - 1U;

  EXPECT_EQ(kExpectedOffset, table.dyn_symbols_offset());
  EXPECT_EQ(kExpectedCount, table.dyn_symbols_count());
}

}  // namespace testing
}  // namespace crazy
