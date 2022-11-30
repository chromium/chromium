// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crazy_linker_elf_symbols.h"

#include <gtest/gtest.h>
#include <memory>

#include "crazy_linker_elf_hash_table_test_data.h"
#include "crazy_linker_gnu_hash_table_test_data.h"

namespace crazy {
namespace testing {

class ElfSymbolsTest : public ::testing::Test {
 protected:
  ElfSymbolsTest(const ELF::Sym* symbol_table,
                 const char* string_table,
                 uintptr_t dt_elf_hash,
                 uintptr_t dt_gnu_hash)
      : symbols_(symbol_table, string_table, dt_elf_hash, dt_gnu_hash) {}

  ElfSymbols symbols_;
};

class ElfSymbolsElfHashTest : public ElfSymbolsTest {
 public:
  ElfSymbolsElfHashTest()
      : ElfSymbolsTest(kTestElfSymbolTable,
                       kTestElfStringTable,
                       reinterpret_cast<uintptr_t>(kTestElfHashTable),
                       0) {}
};

class ElfSymbolsGnuHashTest : public ElfSymbolsTest {
 public:
  ElfSymbolsGnuHashTest()
      : ElfSymbolsTest(kTestGnuSymbolTable,
                       kTestGnuStringTable,
                       0,
                       reinterpret_cast<uintptr_t>(kTestGnuHashTable)) {}
};

#define CHECK_SYMBOL_BY_NAME(name, offset, address, size) \
  sym = symbols_.LookupByName(name);                      \
  EXPECT_TRUE(sym) << name;                               \
  EXPECT_EQ((address), sym->st_value) << name;            \
  EXPECT_EQ((size), sym->st_size) << name;

#define CHECK_SYMBOL_BY_ADDRESS(name, offset, address, size)            \
  sym = symbols_.LookupByAddress(reinterpret_cast<void*>(address), 0);  \
  EXPECT_TRUE(sym) << name;                                             \
  EXPECT_STREQ((name), symbols_.string_table() + sym->st_name) << name; \
  EXPECT_EQ((address), sym->st_value) << name;

#define CHECK_SYMBOL_BY_NEAREST_ADDRESS(name, offset, address, size)           \
  EXPECT_TRUE(                                                                 \
      symbols_.LookupNearestByAddress(reinterpret_cast<void*>((address)-2), 0, \
                                      &sym_name, &sym_addr, &sym_size))        \
      << name;                                                                 \
  EXPECT_STREQ((name), sym_name) << name;                                      \
  EXPECT_EQ((address), reinterpret_cast<uintptr_t>(sym_addr)) << name;         \
  EXPECT_EQ((size), sym_size) << name;

TEST_F(ElfSymbolsElfHashTest, LookupByName) {
  const ELF::Sym* sym;
  LIST_ELF_SYMBOLS_TestElf(CHECK_SYMBOL_BY_NAME);
}

TEST_F(ElfSymbolsElfHashTest, LookupByAddress) {
  const ELF::Sym* sym;
  LIST_ELF_SYMBOLS_TestElf(CHECK_SYMBOL_BY_ADDRESS);
}

TEST_F(ElfSymbolsElfHashTest, LookupNearestByAddress) {
  const char* sym_name;
  void* sym_addr;
  size_t sym_size;
  LIST_ELF_SYMBOLS_TestElf(CHECK_SYMBOL_BY_NEAREST_ADDRESS);
}

TEST_F(ElfSymbolsGnuHashTest, LookupByName) {
  const ELF::Sym* sym;
  LIST_ELF_SYMBOLS_TestGnu(CHECK_SYMBOL_BY_NAME);
}

TEST_F(ElfSymbolsGnuHashTest, LookupByAddress) {
  const ELF::Sym* sym;
  LIST_ELF_SYMBOLS_TestGnu(CHECK_SYMBOL_BY_ADDRESS);
}

TEST_F(ElfSymbolsGnuHashTest, LookupNearestByAddress) {
  const char* sym_name;
  void* sym_addr;
  size_t sym_size;
  LIST_ELF_SYMBOLS_TestGnu(CHECK_SYMBOL_BY_NEAREST_ADDRESS);
}

}  // namespace testing
}  // namespace crazy
