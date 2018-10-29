// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRAZY_LINKER_ELF_HASH_TABLE_TEST_DATA_H
#define CRAZY_LINKER_ELF_HASH_TABLE_TEST_DATA_H

// clang-format off
// BEGIN_AUTO_GENERATED [generate_test_elf_hash_tables.py] DO NOT EDIT!!
//

namespace crazy {
namespace testing {

// SysV ELF hash table: num_buckets=4 num_chain=16
//
// idx symbol               hash      bucket  chain
//   0 <STN_UNDEF>
//   1 isnan                0070a47e  2       5
//   2 freelocal            0bc334fc  0       4
//   3 hcreate_             0a8b8c4f  3       6
//   4 getopt_long_onl      0f256dbc  0       12
//   5 endrpcen             04b96f7e  2       7
//   6 pthread_mutex_lock   0de6a18b  3       0
//   7 isinf                0070a046  2       9
//   8 setrlimi             0cb929a9  1       11
//   9 getspen              0dcba6de  2       10
//  10 umoun                007c46be  2       13
//  11 strsigna             0b99fbe1  1       0
//  12 listxatt             00abef84  0       15
//  13 gettyen              0dcbbfde  2       14
//  14 uselib               07c9c2f2  2       0
//  15 cfsetispeed          0b63b274  0       0
//
// Buckets: 2, 8, 1, 3
//
static const char kTestElfStringTable[145] = {
    '\0','i','s','n','a','n','\0','f','r','e','e','l','o','c','a','l','\0','h',
    'c','r','e','a','t','e','_','\0','g','e','t','o','p','t','_','l','o','n',
    'g','_','o','n','l','\0','e','n','d','r','p','c','e','n','\0','p','t','h',
    'r','e','a','d','_','m','u','t','e','x','_','l','o','c','k','\0','i','s',
    'i','n','f','\0','s','e','t','r','l','i','m','i','\0','g','e','t','s','p',
    'e','n','\0','u','m','o','u','n','\0','s','t','r','s','i','g','n','a','\0',
    'l','i','s','t','x','a','t','t','\0','g','e','t','t','y','e','n','\0','u',
    's','e','l','i','b','\0','c','f','s','e','t','i','s','p','e','e','d','\0',
    '\0'};

// Auto-generated macro used to list all symbols
// XX must be a macro that takes the following parameters:
//   name: symbol name (quoted).
//   str_offset: symbol name offset in string table
//   address: virtual address.
//   size: size in bytes
#define LIST_ELF_SYMBOLS_TestElf(XX) \
    XX("isnan", 1, 0x10000, 16) \
    XX("freelocal", 7, 0x10020, 16) \
    XX("hcreate_", 17, 0x10040, 16) \
    XX("getopt_long_onl", 26, 0x10060, 16) \
    XX("endrpcen", 42, 0x10080, 16) \
    XX("pthread_mutex_lock", 51, 0x100a0, 16) \
    XX("isinf", 70, 0x100c0, 16) \
    XX("setrlimi", 76, 0x100e0, 16) \
    XX("getspen", 85, 0x10100, 16) \
    XX("umoun", 93, 0x10120, 16) \
    XX("strsigna", 99, 0x10140, 16) \
    XX("listxatt", 108, 0x10160, 16) \
    XX("gettyen", 117, 0x10180, 16) \
    XX("uselib", 125, 0x101a0, 16) \
    XX("cfsetispeed", 132, 0x101c0, 16) \
    // END OF LIST

// NOTE: ELF32_Sym and ELF64_Sym have very different layout.
#if UINTPTR_MAX == UINT32_MAX  // ELF32_Sym
#  define DEFINE_ELF_SYMBOL(name, name_offset, address, size) \
    { (name_offset), (address), (size), ELF_ST_INFO(STB_GLOBAL, STT_FUNC), \
      0 /* other */, 1 /* shndx */ },
#else  // ELF64_Sym
#  define DEFINE_ELF_SYMBOL(name, name_offset, address, size) \
    { (name_offset), ELF_ST_INFO(STB_GLOBAL, STT_FUNC), \
      0 /* other */, 1 /* shndx */, (address), (size) },
#endif  // !ELF64_Sym
static const ELF::Sym kTestElfSymbolTable[] = {
    { 0 },  // ST_UNDEF
    LIST_ELF_SYMBOLS_TestElf(DEFINE_ELF_SYMBOL)
};
#undef DEFINE_ELF_SYMBOL

static const uint32_t kTestElfHashTable[] = {
    4,  // num_buckets
    16,  // num_chain
    // Buckets
    0x00000002, 0x00000008, 0x00000001, 0x00000003,
    // Chain
    0x00000000, 0x00000005, 0x00000004, 0x00000006, 0x0000000c, 0x00000007,
    0x00000000, 0x00000009, 0x0000000b, 0x0000000a, 0x0000000d, 0x00000000,
    0x0000000f, 0x0000000e, 0x00000000, 0x00000000,
};

}  // namespace testing
}  // namespace crazy

// END_AUTO_GENERATED_CODE [generate_test_elf_hash_tables.py]
// clang-format on

#endif  // CRAZY_LINKER_ELF_HASH_TABLE_TEST_DATA_H
