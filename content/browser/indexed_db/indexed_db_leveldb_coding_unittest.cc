// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <utility>
#include <vector>

#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "components/services/storage/indexed_db/scopes/varint_coding.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;
using base::StringPiece;
using blink::IndexedDBKey;
using blink::IndexedDBKeyPath;

namespace content {

namespace {

static IndexedDBKey CreateArrayIDBKey() {
  return IndexedDBKey(IndexedDBKey::KeyArray());
}

static IndexedDBKey CreateArrayIDBKey(const IndexedDBKey& key1) {
  IndexedDBKey::KeyArray array = {key1};
  return IndexedDBKey(std::move(array));
}

static IndexedDBKey CreateArrayIDBKey(const IndexedDBKey& key1,
                                      const IndexedDBKey& key2) {
  IndexedDBKey::KeyArray array = {key1, key2};
  return IndexedDBKey(std::move(array));
}

static std::string WrappedEncodeByte(char value) {
  std::string buffer;
  EncodeByte(value, &buffer);
  return buffer;
}

TEST(IndexedDBLevelDBCodingTest, EncodeByte) {
  std::string expected;
  expected.push_back(0);
  unsigned char c;

  c = 0;
  expected[0] = c;
  EXPECT_EQ(expected, WrappedEncodeByte(c));

  c = 1;
  expected[0] = c;
  EXPECT_EQ(expected, WrappedEncodeByte(c));

  c = 255;
  expected[0] = c;
  EXPECT_EQ(expected, WrappedEncodeByte(c));
}

TEST(IndexedDBLevelDBCodingTest, DecodeByte) {
  std::vector<unsigned char> test_cases = {0, 1, 255};

  for (size_t i = 0; i < test_cases.size(); ++i) {
    unsigned char n = test_cases[i];
    std::string v;
    EncodeByte(n, &v);

    unsigned char res;
    ASSERT_GT(v.size(), 0u);
    StringPiece slice(v);
    EXPECT_TRUE(DecodeByte(&slice, &res));
    EXPECT_EQ(n, res);
    EXPECT_TRUE(slice.empty());
  }

  {
    StringPiece slice;
    unsigned char value;
    EXPECT_FALSE(DecodeByte(&slice, &value));
  }
}

static std::string WrappedEncodeBool(bool value) {
  std::string buffer;
  EncodeBool(value, &buffer);
  return buffer;
}

TEST(IndexedDBLevelDBCodingTest, EncodeBool) {
  {
    std::string expected;
    expected.push_back(1);
    EXPECT_EQ(expected, WrappedEncodeBool(true));
  }
  {
    std::string expected;
    expected.push_back(0);
    EXPECT_EQ(expected, WrappedEncodeBool(false));
  }
}

static int CompareKeys(const std::string& a, const std::string& b) {
  DCHECK(!a.empty());
  DCHECK(!b.empty());

  StringPiece slice_a(a);
  StringPiece slice_b(b);
  bool ok;
  int result = CompareEncodedIDBKeys(&slice_a, &slice_b, &ok);
  EXPECT_TRUE(ok);
  return result;
}

TEST(IndexedDBLevelDBCodingTest, MaxIDBKey) {
  std::string max_key = MaxIDBKey();

  std::string min_key = MinIDBKey();
  std::string array_key;
  EncodeIDBKey(IndexedDBKey(IndexedDBKey::KeyArray()), &array_key);
  std::string binary_key;
  EncodeIDBKey(IndexedDBKey(std::string("\x00\x01\x02")), &binary_key);
  std::string string_key;
  EncodeIDBKey(IndexedDBKey(ASCIIToUTF16("Hello world")), &string_key);
  std::string number_key;
  EncodeIDBKey(IndexedDBKey(3.14, blink::mojom::IDBKeyType::Number),
               &number_key);
  std::string date_key;
  EncodeIDBKey(IndexedDBKey(1000000, blink::mojom::IDBKeyType::Date),
               &date_key);

  EXPECT_GT(CompareKeys(max_key, min_key), 0);
  EXPECT_GT(CompareKeys(max_key, array_key), 0);
  EXPECT_GT(CompareKeys(max_key, binary_key), 0);
  EXPECT_GT(CompareKeys(max_key, string_key), 0);
  EXPECT_GT(CompareKeys(max_key, number_key), 0);
  EXPECT_GT(CompareKeys(max_key, date_key), 0);
}

TEST(IndexedDBLevelDBCodingTest, MinIDBKey) {
  std::string min_key = MinIDBKey();

  std::string max_key = MaxIDBKey();
  std::string array_key;
  EncodeIDBKey(IndexedDBKey(IndexedDBKey::KeyArray()), &array_key);
  std::string binary_key;
  EncodeIDBKey(IndexedDBKey(std::string("\x00\x01\x02")), &binary_key);
  std::string string_key;
  EncodeIDBKey(IndexedDBKey(ASCIIToUTF16("Hello world")), &string_key);
  std::string number_key;
  EncodeIDBKey(IndexedDBKey(3.14, blink::mojom::IDBKeyType::Number),
               &number_key);
  std::string date_key;
  EncodeIDBKey(IndexedDBKey(1000000, blink::mojom::IDBKeyType::Date),
               &date_key);

  EXPECT_LT(CompareKeys(min_key, max_key), 0);
  EXPECT_LT(CompareKeys(min_key, array_key), 0);
  EXPECT_LT(CompareKeys(min_key, binary_key), 0);
  EXPECT_LT(CompareKeys(min_key, string_key), 0);
  EXPECT_LT(CompareKeys(min_key, number_key), 0);
  EXPECT_LT(CompareKeys(min_key, date_key), 0);
}

static std::string WrappedEncodeInt(int64_t value) {
  std::string buffer;
  EncodeInt(value, &buffer);
  return buffer;
}

TEST(IndexedDBLevelDBCodingTest, EncodeInt) {
  EXPECT_EQ(1u, WrappedEncodeInt(0).size());
  EXPECT_EQ(1u, WrappedEncodeInt(1).size());
  EXPECT_EQ(1u, WrappedEncodeInt(255).size());
  EXPECT_EQ(2u, WrappedEncodeInt(256).size());
  EXPECT_EQ(4u, WrappedEncodeInt(0xffffffff).size());
#ifdef NDEBUG
  EXPECT_EQ(8u, WrappedEncodeInt(-1).size());
#endif
}

TEST(IndexedDBLevelDBCodingTest, DecodeBool) {
  {
    std::string encoded;
    encoded.push_back(1);
    StringPiece slice(encoded);
    bool value;
    EXPECT_TRUE(DecodeBool(&slice, &value));
    EXPECT_TRUE(value);
    EXPECT_TRUE(slice.empty());
  }
  {
    std::string encoded;
    encoded.push_back(0);
    StringPiece slice(encoded);
    bool value;
    EXPECT_TRUE(DecodeBool(&slice, &value));
    EXPECT_FALSE(value);
    EXPECT_TRUE(slice.empty());
  }
  {
    StringPiece slice;
    bool value;
    EXPECT_FALSE(DecodeBool(&slice, &value));
  }
}

TEST(IndexedDBLevelDBCodingTest, DecodeInt) {
  std::vector<int64_t> test_cases = {
      0,
      1,
      255,
      256,
      65535,
      655536,
      7711192431755665792ll,
      0x7fffffffffffffffll,
#ifdef NDEBUG
      -3,
#endif
  };

  for (size_t i = 0; i < test_cases.size(); ++i) {
    int64_t n = test_cases[i];
    std::string v = WrappedEncodeInt(n);
    ASSERT_GT(v.size(), 0u);
    StringPiece slice(v);
    int64_t value;
    EXPECT_TRUE(DecodeInt(&slice, &value));
    EXPECT_EQ(n, value);
    EXPECT_TRUE(slice.empty());

    // Verify decoding at an offset, to detect unaligned memory access.
    v.insert(v.begin(), 1u, static_cast<char>(0));
    slice = StringPiece(&*v.begin() + 1, v.size() - 1);
    EXPECT_TRUE(DecodeInt(&slice, &value));
    EXPECT_EQ(n, value);
    EXPECT_TRUE(slice.empty());
  }
  {
    StringPiece slice;
    int64_t value;
    EXPECT_FALSE(DecodeInt(&slice, &value));
  }
}

static std::string WrappedEncodeString(base::string16 value) {
  std::string buffer;
  EncodeString(value, &buffer);
  return buffer;
}

TEST(IndexedDBLevelDBCodingTest, EncodeString) {
  const base::char16 test_string_a[] = {'f', 'o', 'o', '\0'};
  const base::char16 test_string_b[] = {0xdead, 0xbeef, '\0'};

  EXPECT_EQ(0u, WrappedEncodeString(ASCIIToUTF16("")).size());
  EXPECT_EQ(2u, WrappedEncodeString(ASCIIToUTF16("a")).size());
  EXPECT_EQ(6u, WrappedEncodeString(ASCIIToUTF16("foo")).size());
  EXPECT_EQ(6u, WrappedEncodeString(base::string16(test_string_a)).size());
  EXPECT_EQ(4u, WrappedEncodeString(base::string16(test_string_b)).size());
}

TEST(IndexedDBLevelDBCodingTest, DecodeString) {
  const base::char16 test_string_a[] = {'f', 'o', 'o', '\0'};
  const base::char16 test_string_b[] = {0xdead, 0xbeef, '\0'};

  std::vector<base::string16> test_cases = {base::string16(), ASCIIToUTF16("a"),
                                            ASCIIToUTF16("foo"), test_string_a,
                                            test_string_b};

  for (size_t i = 0; i < test_cases.size(); ++i) {
    const base::string16& test_case = test_cases[i];
    std::string v = WrappedEncodeString(test_case);

    StringPiece slice;
    if (v.size()) {
      slice = StringPiece(&*v.begin(), v.size());
    }

    base::string16 result;
    EXPECT_TRUE(DecodeString(&slice, &result));
    EXPECT_EQ(test_case, result);
    EXPECT_TRUE(slice.empty());

    // Verify decoding at an offset, to detect unaligned memory access.
    v.insert(v.begin(), 1u, static_cast<char>(0));
    slice = StringPiece(&*v.begin() + 1, v.size() - 1);
    EXPECT_TRUE(DecodeString(&slice, &result));
    EXPECT_EQ(test_case, result);
    EXPECT_TRUE(slice.empty());
  }
}

static std::string WrappedEncodeStringWithLength(base::string16 value) {
  std::string buffer;
  EncodeStringWithLength(value, &buffer);
  return buffer;
}

TEST(IndexedDBLevelDBCodingTest, EncodeStringWithLength) {
  const base::char16 test_string_a[] = {'f', 'o', 'o', '\0'};
  const base::char16 test_string_b[] = {0xdead, 0xbeef, '\0'};

  EXPECT_EQ(1u, WrappedEncodeStringWithLength(base::string16()).size());
  EXPECT_EQ(3u, WrappedEncodeStringWithLength(ASCIIToUTF16("a")).size());
  EXPECT_EQ(
      7u, WrappedEncodeStringWithLength(base::string16(test_string_a)).size());
  EXPECT_EQ(
      5u, WrappedEncodeStringWithLength(base::string16(test_string_b)).size());
}

TEST(IndexedDBLevelDBCodingTest, DecodeStringWithLength) {
  const base::char16 test_string_a[] = {'f', 'o', 'o', '\0'};
  const base::char16 test_string_b[] = {0xdead, 0xbeef, '\0'};

  const int kLongStringLen = 1234;
  base::char16 long_string[kLongStringLen + 1];
  for (int i = 0; i < kLongStringLen; ++i)
    long_string[i] = i;
  long_string[kLongStringLen] = 0;

  std::vector<base::string16> test_cases = {ASCIIToUTF16(""),
                                            ASCIIToUTF16("a"),
                                            ASCIIToUTF16("foo"),
                                            base::string16(test_string_a),
                                            base::string16(test_string_b),
                                            base::string16(long_string)};

  for (size_t i = 0; i < test_cases.size(); ++i) {
    base::string16 s = test_cases[i];
    std::string v = WrappedEncodeStringWithLength(s);
    ASSERT_GT(v.size(), 0u);
    StringPiece slice(v);
    base::string16 res;
    EXPECT_TRUE(DecodeStringWithLength(&slice, &res));
    EXPECT_EQ(s, res);
    EXPECT_TRUE(slice.empty());

    slice = StringPiece(&*v.begin(), v.size() - 1);
    EXPECT_FALSE(DecodeStringWithLength(&slice, &res));

    slice = StringPiece(&*v.begin(), static_cast<size_t>(0));
    EXPECT_FALSE(DecodeStringWithLength(&slice, &res));

    // Verify decoding at an offset, to detect unaligned memory access.
    v.insert(v.begin(), 1u, static_cast<char>(0));
    slice = StringPiece(&*v.begin() + 1, v.size() - 1);
    EXPECT_TRUE(DecodeStringWithLength(&slice, &res));
    EXPECT_EQ(s, res);
    EXPECT_TRUE(slice.empty());
  }
}

static int CompareStrings(const std::string& p, const std::string& q) {
  bool ok;
  DCHECK(!p.empty());
  DCHECK(!q.empty());
  StringPiece slice_p(p);
  StringPiece slice_q(q);
  int result = CompareEncodedStringsWithLength(&slice_p, &slice_q, &ok);
  EXPECT_TRUE(ok);
  EXPECT_TRUE(slice_p.empty());
  EXPECT_TRUE(slice_q.empty());
  return result;
}

TEST(IndexedDBLevelDBCodingTest, CompareEncodedStringsWithLength) {
  const base::char16 test_string_a[] = {0x1000, 0x1000, '\0'};
  const base::char16 test_string_b[] = {0x1000, 0x1000, 0x1000, '\0'};
  const base::char16 test_string_c[] = {0x1000, 0x1000, 0x1001, '\0'};
  const base::char16 test_string_d[] = {0x1001, 0x1000, 0x1000, '\0'};
  const base::char16 test_string_e[] = {0xd834, 0xdd1e, '\0'};
  const base::char16 test_string_f[] = {0xfffd, '\0'};

  std::vector<base::string16> test_cases = {
      ASCIIToUTF16(""),
      ASCIIToUTF16("a"),
      ASCIIToUTF16("b"),
      ASCIIToUTF16("baaa"),
      ASCIIToUTF16("baab"),
      ASCIIToUTF16("c"),
      base::string16(test_string_a),
      base::string16(test_string_b),
      base::string16(test_string_c),
      base::string16(test_string_d),
      base::string16(test_string_e),
      base::string16(test_string_f),
  };

  for (size_t i = 0; i < test_cases.size() - 1; ++i) {
    base::string16 a = test_cases[i];
    base::string16 b = test_cases[i + 1];

    EXPECT_LT(a.compare(b), 0);
    EXPECT_GT(b.compare(a), 0);
    EXPECT_EQ(a.compare(a), 0);
    EXPECT_EQ(b.compare(b), 0);

    std::string encoded_a = WrappedEncodeStringWithLength(a);
    EXPECT_TRUE(encoded_a.size());
    std::string encoded_b = WrappedEncodeStringWithLength(b);
    EXPECT_TRUE(encoded_a.size());

    EXPECT_LT(CompareStrings(encoded_a, encoded_b), 0);
    EXPECT_GT(CompareStrings(encoded_b, encoded_a), 0);
    EXPECT_EQ(CompareStrings(encoded_a, encoded_a), 0);
    EXPECT_EQ(CompareStrings(encoded_b, encoded_b), 0);
  }
}

static std::string WrappedEncodeBinary(const std::string& value) {
  std::string buffer;
  EncodeBinary(value, &buffer);
  return buffer;
}

TEST(IndexedDBLevelDBCodingTest, EncodeBinary) {
  const unsigned char binary_data[] = {0x00, 0x01, 0xfe, 0xff};
  EXPECT_EQ(
      1u,
      WrappedEncodeBinary(std::string(binary_data, binary_data + 0)).size());
  EXPECT_EQ(
      2u,
      WrappedEncodeBinary(std::string(binary_data, binary_data + 1)).size());
  EXPECT_EQ(
      5u,
      WrappedEncodeBinary(std::string(binary_data, binary_data + 4)).size());
}

TEST(IndexedDBLevelDBCodingTest, DecodeBinary) {
  const unsigned char binary_data[] = { 0x00, 0x01, 0xfe, 0xff };

  std::vector<std::string> test_cases = {
      std::string(binary_data, binary_data + 0),
      std::string(binary_data, binary_data + 1),
      std::string(binary_data, binary_data + 4)};

  for (size_t i = 0; i < test_cases.size(); ++i) {
    std::string value = test_cases[i];
    std::string v = WrappedEncodeBinary(value);
    ASSERT_GT(v.size(), 0u);
    StringPiece slice(v);
    std::string result;
    EXPECT_TRUE(DecodeBinary(&slice, &result));
    EXPECT_EQ(value, result);
    EXPECT_TRUE(slice.empty());

    slice = StringPiece(&*v.begin(), v.size() - 1);
    EXPECT_FALSE(DecodeBinary(&slice, &result));

    slice = StringPiece(&*v.begin(), static_cast<size_t>(0));
    EXPECT_FALSE(DecodeBinary(&slice, &result));

    // Verify decoding at an offset, to detect unaligned memory access.
    v.insert(v.begin(), 1u, static_cast<char>(0));
    slice = StringPiece(&*v.begin() + 1, v.size() - 1);
    EXPECT_TRUE(DecodeBinary(&slice, &result));
    EXPECT_EQ(value, result);
    EXPECT_TRUE(slice.empty());
  }
}

static std::string WrappedEncodeDouble(double value) {
  std::string buffer;
  EncodeDouble(value, &buffer);
  return buffer;
}

TEST(IndexedDBLevelDBCodingTest, EncodeDouble) {
  EXPECT_EQ(8u, WrappedEncodeDouble(0).size());
  EXPECT_EQ(8u, WrappedEncodeDouble(3.14).size());
}

TEST(IndexedDBLevelDBCodingTest, DecodeDouble) {
  std::vector<double> test_cases = {3.14, -3.14};

  for (size_t i = 0; i < test_cases.size(); ++i) {
    double value = test_cases[i];
    std::string v = WrappedEncodeDouble(value);
    ASSERT_GT(v.size(), 0u);
    StringPiece slice(v);
    double result;
    EXPECT_TRUE(DecodeDouble(&slice, &result));
    EXPECT_EQ(value, result);
    EXPECT_TRUE(slice.empty());

    slice = StringPiece(&*v.begin(), v.size() - 1);
    EXPECT_FALSE(DecodeDouble(&slice, &result));

    slice = StringPiece(&*v.begin(), static_cast<size_t>(0));
    EXPECT_FALSE(DecodeDouble(&slice, &result));

    // Verify decoding at an offset, to detect unaligned memory access.
    v.insert(v.begin(), 1u, static_cast<char>(0));
    slice = StringPiece(&*v.begin() + 1, v.size() - 1);
    EXPECT_TRUE(DecodeDouble(&slice, &result));
    EXPECT_EQ(value, result);
    EXPECT_TRUE(slice.empty());
  }
}

TEST(IndexedDBLevelDBCodingTest, EncodeDecodeIDBKey) {
  IndexedDBKey expected_key;
  std::unique_ptr<IndexedDBKey> decoded_key;
  std::string v;
  StringPiece slice;

  std::vector<IndexedDBKey> test_cases = {
      IndexedDBKey(1234, blink::mojom::IDBKeyType::Number),
      IndexedDBKey(7890, blink::mojom::IDBKeyType::Date),
      IndexedDBKey(ASCIIToUTF16("Hello World!")),
      IndexedDBKey(std::string("\x01\x02")),
      IndexedDBKey(IndexedDBKey::KeyArray())};

  IndexedDBKey::KeyArray array = {
      IndexedDBKey(1234, blink::mojom::IDBKeyType::Number),
      IndexedDBKey(7890, blink::mojom::IDBKeyType::Date),
      IndexedDBKey(ASCIIToUTF16("Hello World!")),
      IndexedDBKey(std::string("\x01\x02")),
      IndexedDBKey(IndexedDBKey::KeyArray())};
  test_cases.push_back(IndexedDBKey(std::move(array)));

  for (size_t i = 0; i < test_cases.size(); ++i) {
    expected_key = test_cases[i];
    v.clear();
    EncodeIDBKey(expected_key, &v);
    slice = StringPiece(&*v.begin(), v.size());
    EXPECT_TRUE(DecodeIDBKey(&slice, &decoded_key));
    EXPECT_TRUE(decoded_key->Equals(expected_key));
    EXPECT_TRUE(slice.empty());

    slice = StringPiece(&*v.begin(), v.size() - 1);
    EXPECT_FALSE(DecodeIDBKey(&slice, &decoded_key));

    slice = StringPiece(&*v.begin(), static_cast<size_t>(0));
    EXPECT_FALSE(DecodeIDBKey(&slice, &decoded_key));
  }
}

static std::string WrappedEncodeIDBKeyPath(const IndexedDBKeyPath& value) {
  std::string buffer;
  EncodeIDBKeyPath(value, &buffer);
  return buffer;
}

TEST(IndexedDBLevelDBCodingTest, EncodeDecodeIDBKeyPath) {
  std::vector<IndexedDBKeyPath> key_paths;
  std::vector<std::string> encoded_paths;

  {
    key_paths.push_back(IndexedDBKeyPath());
    char expected[] = {0, 0,  // Header
                       0      // Type is null
    };
    encoded_paths.push_back(
        std::string(expected, expected + base::size(expected)));
  }

  {
    key_paths.push_back(IndexedDBKeyPath(base::string16()));
    char expected[] = {0, 0,  // Header
                       1,     // Type is string
                       0      // Length is 0
    };
    encoded_paths.push_back(
        std::string(expected, expected + base::size(expected)));
  }

  {
    key_paths.push_back(IndexedDBKeyPath(ASCIIToUTF16("foo")));
    char expected[] = {0, 0,                      // Header
                       1,                         // Type is string
                       3, 0, 'f', 0, 'o', 0, 'o'  // String length 3, UTF-16BE
    };
    encoded_paths.push_back(
        std::string(expected, expected + base::size(expected)));
  }

  {
    key_paths.push_back(IndexedDBKeyPath(ASCIIToUTF16("foo.bar")));
    char expected[] = {0, 0,  // Header
                       1,     // Type is string
                       7, 0, 'f', 0, 'o', 0, 'o', 0, '.', 0, 'b', 0, 'a', 0,
                       'r'  // String length 7, UTF-16BE
    };
    encoded_paths.push_back(
        std::string(expected, expected + base::size(expected)));
  }

  {
    std::vector<base::string16> array = {base::string16(), ASCIIToUTF16("foo"),
                                         ASCIIToUTF16("foo.bar")};

    key_paths.push_back(IndexedDBKeyPath(array));
    char expected[] = {0, 0,                       // Header
                       2, 3,                       // Type is array, length is 3
                       0,                          // Member 1 (String length 0)
                       3, 0, 'f', 0, 'o', 0, 'o',  // Member 2 (String length 3)
                       7, 0, 'f', 0, 'o', 0, 'o', 0, '.', 0, 'b', 0, 'a', 0,
                       'r'  // Member 3 (String length 7)
    };
    encoded_paths.push_back(
        std::string(expected, expected + base::size(expected)));
  }

  ASSERT_EQ(key_paths.size(), encoded_paths.size());
  for (size_t i = 0; i < key_paths.size(); ++i) {
    IndexedDBKeyPath key_path = key_paths[i];
    std::string encoded = encoded_paths[i];

    std::string v = WrappedEncodeIDBKeyPath(key_path);
    EXPECT_EQ(encoded, v);

    StringPiece slice(encoded);
    IndexedDBKeyPath decoded;
    EXPECT_TRUE(DecodeIDBKeyPath(&slice, &decoded));
    EXPECT_EQ(key_path, decoded);
    EXPECT_TRUE(slice.empty());
  }
}

TEST(IndexedDBLevelDBCodingTest, EncodeDecodeBlobJournal) {
  std::vector<IndexedDBKeyPath> key_paths;
  std::vector<std::string> encoded_paths;

  std::vector<BlobJournalType> journals;

  {  // Empty journal
    journals.push_back({});
  }

  {  // One item
    journals.push_back({{4, 7}});
  }

  {  // kAllBlobsKey
    journals.push_back({{5, DatabaseMetaDataKey::kAllBlobsKey}});
  }

  {  // A bunch of items
    journals.push_back(
        {{4, 7}, {5, 6}, {4, 5}, {4, 4}, {1, 12}, {4, 3}, {15, 14}});
  }

  for (const auto& journal_iter : journals) {
    std::string encoding;
    EncodeBlobJournal(journal_iter, &encoding);
    StringPiece slice(encoding);
    BlobJournalType journal_out;
    EXPECT_TRUE(DecodeBlobJournal(&slice, &journal_out));
    EXPECT_EQ(journal_iter, journal_out);
  }

  journals.clear();

  {  // Illegal database id
    journals.push_back({{0, 3}});
  }

  {  // Illegal blob id
    journals.push_back({{4, 0}});
  }

  for (const auto& journal_iter : journals) {
    std::string encoding;
    EncodeBlobJournal(journal_iter, &encoding);
    StringPiece slice(encoding);
    BlobJournalType journal_out;
    EXPECT_FALSE(DecodeBlobJournal(&slice, &journal_out));
  }
}

TEST(IndexedDBLevelDBCodingTest, DecodeLegacyIDBKeyPath) {
  // Legacy encoding of string key paths.
  std::vector<IndexedDBKeyPath> key_paths;
  std::vector<std::string> encoded_paths;

  {
    key_paths.push_back(IndexedDBKeyPath(base::string16()));
    encoded_paths.push_back(std::string());
  }
  {
    key_paths.push_back(IndexedDBKeyPath(ASCIIToUTF16("foo")));
    char expected[] = {0, 'f', 0, 'o', 0, 'o'};
    encoded_paths.push_back(std::string(expected, base::size(expected)));
  }
  {
    key_paths.push_back(IndexedDBKeyPath(ASCIIToUTF16("foo.bar")));
    char expected[] = {0, 'f', 0, 'o', 0, 'o', 0, '.', 0, 'b', 0, 'a', 0, 'r'};
    encoded_paths.push_back(std::string(expected, base::size(expected)));
  }

  ASSERT_EQ(key_paths.size(), encoded_paths.size());
  for (size_t i = 0; i < key_paths.size(); ++i) {
    IndexedDBKeyPath key_path = key_paths[i];
    std::string encoded = encoded_paths[i];

    StringPiece slice(encoded);
    IndexedDBKeyPath decoded;
    EXPECT_TRUE(DecodeIDBKeyPath(&slice, &decoded));
    EXPECT_EQ(key_path, decoded);
    EXPECT_TRUE(slice.empty());
  }
}

TEST(IndexedDBLevelDBCodingTest, ExtractAndCompareIDBKeys) {
  std::vector<IndexedDBKey> keys = {
      IndexedDBKey(-10, blink::mojom::IDBKeyType::Number),
      IndexedDBKey(0, blink::mojom::IDBKeyType::Number),
      IndexedDBKey(3.14, blink::mojom::IDBKeyType::Number),

      IndexedDBKey(0, blink::mojom::IDBKeyType::Date),
      IndexedDBKey(100, blink::mojom::IDBKeyType::Date),
      IndexedDBKey(100000, blink::mojom::IDBKeyType::Date),

      IndexedDBKey(ASCIIToUTF16("")),
      IndexedDBKey(ASCIIToUTF16("a")),
      IndexedDBKey(ASCIIToUTF16("b")),
      IndexedDBKey(ASCIIToUTF16("baaa")),
      IndexedDBKey(ASCIIToUTF16("baab")),
      IndexedDBKey(ASCIIToUTF16("c")),

      IndexedDBKey(std::string()),
      IndexedDBKey(std::string("\x01")),
      IndexedDBKey(std::string("\x01\x01")),
      IndexedDBKey(std::string("\x01\x02")),
      IndexedDBKey(std::string("\x02")),
      IndexedDBKey(std::string("\x02\x01")),
      IndexedDBKey(std::string("\x02\x02")),
      IndexedDBKey(std::string("\xff")),

      CreateArrayIDBKey(),

      CreateArrayIDBKey(IndexedDBKey(0, blink::mojom::IDBKeyType::Number)),

      CreateArrayIDBKey(IndexedDBKey(0, blink::mojom::IDBKeyType::Number),
                        IndexedDBKey(3.14, blink::mojom::IDBKeyType::Number)),

      CreateArrayIDBKey(IndexedDBKey(0, blink::mojom::IDBKeyType::Date)),

      CreateArrayIDBKey(IndexedDBKey(0, blink::mojom::IDBKeyType::Date),
                        IndexedDBKey(0, blink::mojom::IDBKeyType::Date)),
      CreateArrayIDBKey(IndexedDBKey(ASCIIToUTF16(""))),
      CreateArrayIDBKey(IndexedDBKey(ASCIIToUTF16("")),
                        IndexedDBKey(ASCIIToUTF16("a"))),
      CreateArrayIDBKey(CreateArrayIDBKey()),
      CreateArrayIDBKey(CreateArrayIDBKey(), CreateArrayIDBKey()),
      CreateArrayIDBKey(CreateArrayIDBKey(CreateArrayIDBKey())),
      CreateArrayIDBKey(
          CreateArrayIDBKey(CreateArrayIDBKey(CreateArrayIDBKey()))),
  };

  for (size_t i = 0; i < keys.size() - 1; ++i) {
    const IndexedDBKey& key_a = keys[i];
    const IndexedDBKey& key_b = keys[i + 1];

    EXPECT_TRUE(key_a.IsLessThan(key_b));

    std::string encoded_a;
    EncodeIDBKey(key_a, &encoded_a);
    EXPECT_TRUE(encoded_a.size());
    std::string encoded_b;
    EncodeIDBKey(key_b, &encoded_b);
    EXPECT_TRUE(encoded_b.size());

    std::string extracted_a;
    std::string extracted_b;
    StringPiece slice;

    slice = StringPiece(encoded_a);
    EXPECT_TRUE(ExtractEncodedIDBKey(&slice, &extracted_a));
    EXPECT_TRUE(slice.empty());
    EXPECT_EQ(encoded_a, extracted_a);

    slice = StringPiece(encoded_b);
    EXPECT_TRUE(ExtractEncodedIDBKey(&slice, &extracted_b));
    EXPECT_TRUE(slice.empty());
    EXPECT_EQ(encoded_b, extracted_b);

    EXPECT_LT(CompareKeys(extracted_a, extracted_b), 0);
    EXPECT_GT(CompareKeys(extracted_b, extracted_a), 0);
    EXPECT_EQ(CompareKeys(extracted_a, extracted_a), 0);
    EXPECT_EQ(CompareKeys(extracted_b, extracted_b), 0);

    slice = StringPiece(&*encoded_a.begin(), encoded_a.size() - 1);
    EXPECT_FALSE(ExtractEncodedIDBKey(&slice, &extracted_a));
  }
}

TEST(IndexedDBLevelDBCodingTest, ComparisonTest) {
  std::vector<std::string> keys = {
      SchemaVersionKey::Encode(),
      MaxDatabaseIdKey::Encode(),
      DatabaseFreeListKey::Encode(0),
      DatabaseFreeListKey::EncodeMaxKey(),
      DatabaseNameKey::Encode("", ASCIIToUTF16("")),
      DatabaseNameKey::Encode("", ASCIIToUTF16("a")),
      DatabaseNameKey::Encode("a", ASCIIToUTF16("a")),

      DatabaseMetaDataKey::Encode(1, DatabaseMetaDataKey::ORIGIN_NAME),

      DatabaseMetaDataKey::Encode(1, DatabaseMetaDataKey::DATABASE_NAME),

      DatabaseMetaDataKey::Encode(1, DatabaseMetaDataKey::USER_STRING_VERSION),

      DatabaseMetaDataKey::Encode(1, DatabaseMetaDataKey::MAX_OBJECT_STORE_ID),

      DatabaseMetaDataKey::Encode(1, DatabaseMetaDataKey::USER_VERSION),

      ObjectStoreMetaDataKey::Encode(1, 1, ObjectStoreMetaDataKey::NAME),

      ObjectStoreMetaDataKey::Encode(1, 1, ObjectStoreMetaDataKey::KEY_PATH),
      ObjectStoreMetaDataKey::Encode(1, 1,
                                     ObjectStoreMetaDataKey::AUTO_INCREMENT),

      ObjectStoreMetaDataKey::Encode(1, 1, ObjectStoreMetaDataKey::EVICTABLE),
      ObjectStoreMetaDataKey::Encode(1, 1,
                                     ObjectStoreMetaDataKey::LAST_VERSION),
      ObjectStoreMetaDataKey::Encode(1, 1,
                                     ObjectStoreMetaDataKey::MAX_INDEX_ID),
      ObjectStoreMetaDataKey::Encode(1, 1,
                                     ObjectStoreMetaDataKey::HAS_KEY_PATH),
      ObjectStoreMetaDataKey::Encode(
          1, 1, ObjectStoreMetaDataKey::KEY_GENERATOR_CURRENT_NUMBER),
      ObjectStoreMetaDataKey::EncodeMaxKey(1, 1),
      ObjectStoreMetaDataKey::EncodeMaxKey(1, 2),
      ObjectStoreMetaDataKey::EncodeMaxKey(1),
      IndexMetaDataKey::Encode(1, 1, 30, IndexMetaDataKey::NAME),
      IndexMetaDataKey::Encode(1, 1, 30, IndexMetaDataKey::UNIQUE),

      IndexMetaDataKey::Encode(1, 1, 30, IndexMetaDataKey::KEY_PATH),

      IndexMetaDataKey::Encode(1, 1, 30, IndexMetaDataKey::MULTI_ENTRY),
      IndexMetaDataKey::Encode(1, 1, 31, 0),
      IndexMetaDataKey::Encode(1, 1, 31, 1),
      IndexMetaDataKey::EncodeMaxKey(1, 1, 31),
      IndexMetaDataKey::EncodeMaxKey(1, 1, 32),
      IndexMetaDataKey::EncodeMaxKey(1, 1),
      IndexMetaDataKey::EncodeMaxKey(1, 2),
      ObjectStoreFreeListKey::Encode(1, 1),
      ObjectStoreFreeListKey::EncodeMaxKey(1),
      IndexFreeListKey::Encode(1, 1, kMinimumIndexId),
      IndexFreeListKey::EncodeMaxKey(1, 1),
      IndexFreeListKey::Encode(1, 2, kMinimumIndexId),
      IndexFreeListKey::EncodeMaxKey(1, 2),
      ObjectStoreNamesKey::Encode(1, ASCIIToUTF16("")),
      ObjectStoreNamesKey::Encode(1, ASCIIToUTF16("a")),
      IndexNamesKey::Encode(1, 1, ASCIIToUTF16("")),
      IndexNamesKey::Encode(1, 1, ASCIIToUTF16("a")),
      IndexNamesKey::Encode(1, 2, ASCIIToUTF16("a")),
      ObjectStoreDataKey::Encode(1, 1, std::string()),
      ObjectStoreDataKey::Encode(1, 1, MinIDBKey()),
      ObjectStoreDataKey::Encode(1, 1, MaxIDBKey()),
      ExistsEntryKey::Encode(1, 1, std::string()),
      ExistsEntryKey::Encode(1, 1, MinIDBKey()),
      ExistsEntryKey::Encode(1, 1, MaxIDBKey()),
      IndexDataKey::Encode(1, 1, 30, MinIDBKey(), std::string(), 0),
      IndexDataKey::Encode(1, 1, 30, MinIDBKey(), MinIDBKey(), 0),
      IndexDataKey::Encode(1, 1, 30, MinIDBKey(), MinIDBKey(), 1),
      IndexDataKey::Encode(1, 1, 30, MinIDBKey(), MaxIDBKey(), 0),
      IndexDataKey::Encode(1, 1, 30, MinIDBKey(), MaxIDBKey(), 1),
      IndexDataKey::Encode(1, 1, 30, MaxIDBKey(), MinIDBKey(), 0),
      IndexDataKey::Encode(1, 1, 30, MaxIDBKey(), MinIDBKey(), 1),
      IndexDataKey::Encode(1, 1, 30, MaxIDBKey(), MaxIDBKey(), 0),
      IndexDataKey::Encode(1, 1, 30, MaxIDBKey(), MaxIDBKey(), 1),
      IndexDataKey::Encode(1, 1, 31, MinIDBKey(), MinIDBKey(), 0),
      IndexDataKey::Encode(1, 2, 30, MinIDBKey(), MinIDBKey(), 0),
      IndexDataKey::EncodeMaxKey(1, 2, std::numeric_limits<int32_t>::max() - 1),
  };

  for (size_t i = 0; i < keys.size(); ++i) {
    EXPECT_EQ(Compare(keys[i], keys[i], false), 0);

    for (size_t j = i + 1; j < keys.size(); ++j) {
      EXPECT_LT(Compare(keys[i], keys[j], false), 0);
      EXPECT_GT(Compare(keys[j], keys[i], false), 0);
    }
  }
}

TEST(IndexedDBLevelDBCodingTest, IndexDataKeyEncodeDecode) {
  std::vector<std::string> keys = {
      IndexDataKey::Encode(1, 1, 30, MinIDBKey(), MinIDBKey(), 0),
      IndexDataKey::Encode(1, 1, 30, MinIDBKey(), MinIDBKey(), 1),

      IndexDataKey::Encode(1, 1, 30, IndexedDBKey(ASCIIToUTF16("user key")),
                           IndexedDBKey(ASCIIToUTF16("primary key"))),
      IndexDataKey::Encode(1, 1, 30, MinIDBKey(), MaxIDBKey(), 0),
      IndexDataKey::Encode(1, 1, 30, MinIDBKey(), MaxIDBKey(), 1),
      IndexDataKey::Encode(1, 1, 30, MaxIDBKey(), MinIDBKey(), 0),
      IndexDataKey::Encode(1, 1, 30, MaxIDBKey(), MinIDBKey(), 1),
      IndexDataKey::Encode(1, 1, 30, MaxIDBKey(), MaxIDBKey(), 0),
      IndexDataKey::Encode(1, 1, 30, MaxIDBKey(), MaxIDBKey(), 1),
      IndexDataKey::Encode(1, 1, 31, MinIDBKey(), MinIDBKey(), 0),
      IndexDataKey::Encode(1, 2, 30, MinIDBKey(), MinIDBKey(), 0),
      IndexDataKey::EncodeMaxKey(1, 2, std::numeric_limits<int32_t>::max() - 1),
  };

  std::vector<IndexDataKey> obj_keys;
  for (const std::string& key : keys) {
    base::StringPiece piece(key);
    IndexDataKey obj_key;
    EXPECT_TRUE(IndexDataKey::Decode(&piece, &obj_key));
    obj_keys.push_back(std::move(obj_key));
  }

  for (size_t i = 0; i < keys.size(); ++i) {
    EXPECT_EQ(keys[i], obj_keys[i].Encode()) << "key at " << i;
  }
}

TEST(IndexedDBLevelDBCodingTest, EncodeVarIntVSEncodeByteTest) {
  std::vector<unsigned char> test_cases = {0, 1, 127};

  for (size_t i = 0; i < test_cases.size(); ++i) {
    unsigned char n = test_cases[i];

    std::string a = WrappedEncodeByte(n);
    std::string b;
    EncodeVarInt(static_cast<int64_t>(n), &b);

    EXPECT_EQ(a.size(), b.size());
    EXPECT_EQ(*a.begin(), *b.begin());
  }
}

}  // namespace

}  // namespace content
