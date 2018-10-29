// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRAZY_LINKER_GNU_HASH_TABLE_TEST_DATA_H
#define CRAZY_LINKER_GNU_HASH_TABLE_TEST_DATA_H

// clang-format off
// BEGIN_AUTO_GENERATED [generate_test_gnu_hash_tables.py] DO NOT EDIT!!
//

namespace crazy {
namespace testing {

// GNU hash table: num_buckets=4 bloom_size=2 bloom_shift=5
//
// idx symbol               hash      bucket  bloom32  bloom64  chain
//
//   0 ST_UNDEF
//   1 cfsetispeed          830acc54  0       0:20:02  1:20:34  830acc54
//   2 strsigna             90f1e4b0  0       1:16:05  0:48:37  90f1e4b0
//   3 hcreate_             4c7e3240  0       0:00:18  1:00:18  4c7e3240
//   4 endrpcen             b6c44714  0       0:20:24  0:20:56  b6c44715
//   5 uselib               2124d3e9  1       1:09:31  1:41:31  2124d3e8
//   6 gettyen              f07bdd25  1       1:05:09  0:37:41  f07bdd24
//   7 umoun                1081e019  1       0:25:00  0:25:00  1081e019
//   8 freelocal            e3364372  2       1:18:27  1:50:27  e3364372
//   9 listxatt             ced3d862  2       1:02:03  1:34:03  ced3d862
//  10 isnan                0fabfd7e  2       1:30:11  1:62:43  0fabfd7e
//  11 isinf                0fabe9de  2       0:30:14  1:30:14  0fabe9de
//  12 setrlimi             12e23bae  2       1:14:29  0:46:29  12e23baf
//  13 getspen              f07b2a7b  3       1:27:19  1:59:19  f07b2a7a
//  14 pthread_mutex_lock   4f152227  3       1:07:17  0:39:17  4f152226
//  15 getopt_long_onl      57b1584f  3       0:15:02  1:15:02  57b1584f
//
// Buckets: 1, 5, 8, 13
//
// Bloom filter (32 bits):
// bit#       24       16        8        0
//      .x....xx ...x.x.. xx...... .....x.x
//      xxx.x... ....xxxx .x..x.x. x.x.xx..
//
//   also as:  0x4314c005 0xe80f4aac
//
// Bloom filter (64 bits):
// bit#       56       48       40       32       24       16        8        0
//      .......x .......x .x....x. x.x..... ..x...x. ...x..x. ........ .......x
//      .x..x... .....x.. ....x.x. .....x.. xx..x... ...xxx.. xx...... ....xx.x
//
//   also as:  0x010142a022120001 0x48040a04c81cc00d
//
static const char kTestGnuStringTable[145] = {
    '\0','c','f','s','e','t','i','s','p','e','e','d','\0','s','t','r','s','i',
    'g','n','a','\0','h','c','r','e','a','t','e','_','\0','e','n','d','r','p',
    'c','e','n','\0','u','s','e','l','i','b','\0','g','e','t','t','y','e','n',
    '\0','u','m','o','u','n','\0','f','r','e','e','l','o','c','a','l','\0','l',
    'i','s','t','x','a','t','t','\0','i','s','n','a','n','\0','i','s','i','n',
    'f','\0','s','e','t','r','l','i','m','i','\0','g','e','t','s','p','e','n',
    '\0','p','t','h','r','e','a','d','_','m','u','t','e','x','_','l','o','c',
    'k','\0','g','e','t','o','p','t','_','l','o','n','g','_','o','n','l','\0',
    '\0'};

// Auto-generated macro used to list all symbols
// XX must be a macro that takes the following parameters:
//   name: symbol name (quoted).
//   str_offset: symbol name offset in string table
//   address: virtual address.
//   size: size in bytes
#define LIST_ELF_SYMBOLS_TestGnu(XX) \
    XX("cfsetispeed", 1, 0x10000, 16) \
    XX("strsigna", 13, 0x10020, 16) \
    XX("hcreate_", 22, 0x10040, 16) \
    XX("endrpcen", 31, 0x10060, 16) \
    XX("uselib", 40, 0x10080, 16) \
    XX("gettyen", 47, 0x100a0, 16) \
    XX("umoun", 55, 0x100c0, 16) \
    XX("freelocal", 61, 0x100e0, 16) \
    XX("listxatt", 71, 0x10100, 16) \
    XX("isnan", 80, 0x10120, 16) \
    XX("isinf", 86, 0x10140, 16) \
    XX("setrlimi", 92, 0x10160, 16) \
    XX("getspen", 101, 0x10180, 16) \
    XX("pthread_mutex_lock", 109, 0x101a0, 16) \
    XX("getopt_long_onl", 128, 0x101c0, 16) \
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
static const ELF::Sym kTestGnuSymbolTable[] = {
    { 0 },  // ST_UNDEF
    LIST_ELF_SYMBOLS_TestGnu(DEFINE_ELF_SYMBOL)
};
#undef DEFINE_ELF_SYMBOL

static const uint32_t kTestGnuHashTable[] = {
    4,  // num_buckets
    1,  // sym_offset
    2,  // bloom_size
    5,  // bloom_shift
    // Bloom filter words
#if UINTPTR_MAX == UINT32_MAX  // 32-bit bloom filter words
    0x4314c005, 0xe80f4aac,
#else  // 64-bits filter bloom words (assumes little-endianess)
    0x22120001, 0x010142a0, 0xc81cc00d, 0x48040a04,
#endif  // bloom filter words
    // Buckets
    0x00000001, 0x00000005, 0x00000008, 0x0000000d,
    // Chain
    0x830acc54, 0x90f1e4b0, 0x4c7e3240, 0xb6c44715, 0x2124d3e8, 0xf07bdd24,
    0x1081e019, 0xe3364372, 0xced3d862, 0x0fabfd7e, 0x0fabe9de, 0x12e23baf,
    0xf07b2a7a, 0x4f152226, 0x57b1584f,
};

}  // namespace testing
}  // namespace crazy

// END_AUTO_GENERATED_CODE [generate_test_gnu_hash_tables.py]
// clang-format on

#endif  // CRAZY_LINKER_GNU_HASH_TABLE_TEST_DATA_H
