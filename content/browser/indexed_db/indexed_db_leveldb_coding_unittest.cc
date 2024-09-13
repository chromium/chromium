// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "components/services/storage/indexed_db/scopes/varint_coding.h"
#include "testing/gtest/include/gtest/gtest.h"

using blink::IndexedDBKey;
using blink::IndexedDBKeyPath;

namespace content::indexed_db {
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
    std::string_view slice(v);
    EXPECT_TRUE(DecodeByte(&slice, &res));
    EXPECT_EQ(n, res);
    EXPECT_TRUE(slice.empty());
  }

  {
    std::string_view slice;
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

  std::string_view slice_a(a);
  std::string_view slice_b(b);
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
  EncodeIDBKey(IndexedDBKey(u"Hello world"), &string_key);
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
  EncodeIDBKey(IndexedDBKey(u"Hello world"), &string_key);
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
    std::string_view slice(encoded);
    bool value;
    EXPECT_TRUE(DecodeBool(&slice, &value));
    EXPECT_TRUE(value);
    EXPECT_TRUE(slice.empty());
  }
  {
    std::string encoded;
    encoded.push_back(0);
    std::string_view slice(encoded);
    bool value;
    EXPECT_TRUE(DecodeBool(&slice, &value));
    EXPECT_FALSE(value);
    EXPECT_TRUE(slice.empty());
  }
  {
    std::string_view slice;
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
    std::string_view slice(v);
    int64_t value;
    EXPECT_TRUE(DecodeInt(&slice, &value));
    EXPECT_EQ(n, value);
    EXPECT_TRUE(slice.empty());

    // Verify decoding at an offset, to detect unaligned memory access.
    v.insert(v.begin(), 1u, static_cast<char>(0));
    slice = std::string_view(v).substr(1u);
    EXPECT_TRUE(DecodeInt(&slice, &value));
    EXPECT_EQ(n, value);
    EXPECT_TRUE(slice.empty());
  }
  {
    std::string_view slice;
    int64_t value;
    EXPECT_FALSE(DecodeInt(&slice, &value));
  }
}

static std::string WrappedEncodeString(std::u16string value) {
  std::string buffer;
  EncodeString(value, &buffer);
  return buffer;
}

TEST(IndexedDBLevelDBCodingTest, EncodeString) {
  const char16_t test_string_a[] = {'f', 'o', 'o', '\0'};
  const char16_t test_string_b[] = {0xdead, 0xbeef, '\0'};

  EXPECT_EQ(0u, WrappedEncodeString(u"").size());
  EXPECT_EQ(2u, WrappedEncodeString(u"a").size());
  EXPECT_EQ(6u, WrappedEncodeString(u"foo").size());
  EXPECT_EQ(6u, WrappedEncodeString(std::u16string(test_string_a)).size());
  EXPECT_EQ(4u, WrappedEncodeString(std::u16string(test_string_b)).size());
}

TEST(IndexedDBLevelDBCodingTest, DecodeString) {
  const char16_t test_string_a[] = {'f', 'o', 'o', '\0'};
  const char16_t test_string_b[] = {0xdead, 0xbeef, '\0'};

  std::vector<std::u16string> test_cases = {u"", u"a", u"foo", test_string_a,
                                            test_string_b};

  for (size_t i = 0; i < test_cases.size(); ++i) {
    const std::u16string& test_case = test_cases[i];
    std::string v = WrappedEncodeString(test_case);

    std::string_view slice;
    if (v.size()) {
      slice = std::string_view(&*v.begin(), v.size());
    }

    std::u16string result;
    EXPECT_TRUE(DecodeString(&slice, &result));
    EXPECT_EQ(test_case, result);
    EXPECT_TRUE(slice.empty());

    // Verify decoding at an offset, to detect unaligned memory access.
    v.insert(v.begin(), 1u, static_cast<char>(0));
    slice = std::string_view(v).substr(1u);
    EXPECT_TRUE(DecodeString(&slice, &result));
    EXPECT_EQ(test_case, result);
    EXPECT_TRUE(slice.empty());
  }
}

static std::string WrappedEncodeStringWithLength(std::u16string value) {
  std::string buffer;
  EncodeStringWithLength(value, &buffer);
  return buffer;
}

TEST(IndexedDBLevelDBCodingTest, EncodeStringWithLength) {
  const char16_t test_string_a[] = {'f', 'o', 'o', '\0'};
  const char16_t test_string_b[] = {0xdead, 0xbeef, '\0'};

  EXPECT_EQ(1u, WrappedEncodeStringWithLength(u"").size());
  EXPECT_EQ(3u, WrappedEncodeStringWithLength(u"a").size());
  EXPECT_EQ(
      7u, WrappedEncodeStringWithLength(std::u16string(test_string_a)).size());
  EXPECT_EQ(
      5u, WrappedEncodeStringWithLength(std::u16string(test_string_b)).size());
}

TEST(IndexedDBLevelDBCodingTest, DecodeStringWithLength) {
  const char16_t test_string_a[] = {'f', 'o', 'o', '\0'};
  const char16_t test_string_b[] = {0xdead, 0xbeef, '\0'};

  const int kLongStringLen = 1234;
  char16_t long_string[kLongStringLen + 1];
  for (int i = 0; i < kLongStringLen; ++i)
    long_string[i] = i;
  long_string[kLongStringLen] = 0;

  std::vector<std::u16string> test_cases = {u"",
                                            u"a",
                                            u"foo",
                                            std::u16string(test_string_a),
                                            std::u16string(test_string_b),
                                            std::u16string(long_string)};

  for (size_t i = 0; i < test_cases.size(); ++i) {
    std::u16string s = test_cases[i];
    std::string v = WrappedEncodeStringWithLength(s);
    ASSERT_GT(v.size(), 0u);
    std::string_view slice(v);
    std::u16string res;
    EXPECT_TRUE(DecodeStringWithLength(&slice, &res));
    EXPECT_EQ(s, res);
    EXPECT_TRUE(slice.empty());

    slice = std::string_view(&*v.begin(), v.size() - 1);
    EXPECT_FALSE(DecodeStringWithLength(&slice, &res));

    slice = std::string_view(&*v.begin(), static_cast<size_t>(0));
    EXPECT_FALSE(DecodeStringWithLength(&slice, &res));

    // Verify decoding at an offset, to detect unaligned memory access.
    v.insert(v.begin(), 1u, static_cast<char>(0));
    slice = std::string_view(v).substr(1u);
    EXPECT_TRUE(DecodeStringWithLength(&slice, &res));
    EXPECT_EQ(s, res);
    EXPECT_TRUE(slice.empty());
  }
}

static int CompareStrings(const std::string& p, const std::string& q) {
  bool ok;
  DCHECK(!p.empty());
  DCHECK(!q.empty());
  std::string_view slice_p(p);
  std::string_view slice_q(q);
  int result = CompareEncodedStringsWithLength(&slice_p, &slice_q, &ok);
  EXPECT_TRUE(ok);
  EXPECT_TRUE(slice_p.empty());
  EXPECT_TRUE(slice_q.empty());
  return result;
}

TEST(IndexedDBLevelDBCodingTest, CompareEncodedStringsWithLength) {
  const char16_t test_string_a[] = {0x1000, 0x1000, '\0'};
  const char16_t test_string_b[] = {0x1000, 0x1000, 0x1000, '\0'};
  const char16_t test_string_c[] = {0x1000, 0x1000, 0x1001, '\0'};
  const char16_t test_string_d[] = {0x1001, 0x1000, 0x1000, '\0'};
  const char16_t test_string_e[] = {0xd834, 0xdd1e, '\0'};
  const char16_t test_string_f[] = {0xfffd, '\0'};

  std::vector<std::u16string> test_cases = {
      u"",
      u"a",
      u"b",
      u"baaa",
      u"baab",
      u"c",
      std::u16string(test_string_a),
      std::u16string(test_string_b),
      std::u16string(test_string_c),
      std::u16string(test_string_d),
      std::u16string(test_string_e),
      std::u16string(test_string_f),
  };

  for (size_t i = 0; i < test_cases.size() - 1; ++i) {
    std::u16string a = test_cases[i];
    std::u16string b = test_cases[i + 1];

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
    std::string_view slice(v);
    std::string result;
    EXPECT_TRUE(DecodeBinary(&slice, &result));
    EXPECT_EQ(value, result);
    EXPECT_TRUE(slice.empty());

    slice = std::string_view(&*v.begin(), v.size() - 1);
    EXPECT_FALSE(DecodeBinary(&slice, &result));

    slice = std::string_view(&*v.begin(), static_cast<size_t>(0));
    EXPECT_FALSE(DecodeBinary(&slice, &result));

    // Verify decoding at an offset, to detect unaligned memory access.
    v.insert(v.begin(), 1u, static_cast<char>(0));
    slice = std::string_view(v).substr(1u);
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
    std::string_view slice(v);
    double result;
    EXPECT_TRUE(DecodeDouble(&slice, &result));
    EXPECT_EQ(value, result);
    EXPECT_TRUE(slice.empty());

    slice = std::string_view(&*v.begin(), v.size() - 1);
    EXPECT_FALSE(DecodeDouble(&slice, &result));

    slice = std::string_view(&*v.begin(), static_cast<size_t>(0));
    EXPECT_FALSE(DecodeDouble(&slice, &result));

    // Verify decoding at an offset, to detect unaligned memory access.
    v.insert(v.begin(), 1u, static_cast<char>(0));
    slice = std::string_view(v).substr(1u);
    EXPECT_TRUE(DecodeDouble(&slice, &result));
    EXPECT_EQ(value, result);
    EXPECT_TRUE(slice.empty());
  }
}

TEST(IndexedDBLevelDBCodingTest, EncodeDecodeIDBKey) {
  IndexedDBKey expected_key;
  std::unique_ptr<IndexedDBKey> decoded_key;
  std::string v;
  std::string_view slice;

  std::vector<IndexedDBKey> test_cases = {
      IndexedDBKey(1234, blink::mojom::IDBKeyType::Number),
      IndexedDBKey(7890, blink::mojom::IDBKeyType::Date),
      IndexedDBKey(u"Hello World!"), IndexedDBKey(std::string("\x01\x02")),
      IndexedDBKey(IndexedDBKey::KeyArray())};

  IndexedDBKey::KeyArray array = {
      IndexedDBKey(1234, blink::mojom::IDBKeyType::Number),
      IndexedDBKey(7890, blink::mojom::IDBKeyType::Date),
      IndexedDBKey(u"Hello World!"), IndexedDBKey(std::string("\x01\x02")),
      IndexedDBKey(IndexedDBKey::KeyArray())};
  test_cases.push_back(IndexedDBKey(std::move(array)));

  for (size_t i = 0; i < test_cases.size(); ++i) {
    expected_key = test_cases[i];
    v.clear();
    EncodeIDBKey(expected_key, &v);
    slice = std::string_view(&*v.begin(), v.size());
    EXPECT_TRUE(DecodeIDBKey(&slice, &decoded_key));
    EXPECT_TRUE(decoded_key->Equals(expected_key));
    EXPECT_TRUE(slice.empty());

    slice = std::string_view(&*v.begin(), v.size() - 1);
    EXPECT_FALSE(DecodeIDBKey(&slice, &decoded_key));

    slice = std::string_view(&*v.begin(), static_cast<size_t>(0));
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
        std::string(expected, expected + std::size(expected)));
  }

  {
    key_paths.push_back(IndexedDBKeyPath(u""));
    char expected[] = {0, 0,  // Header
                       1,     // Type is string
                       0      // Length is 0
    };
    encoded_paths.push_back(
        std::string(expected, expected + std::size(expected)));
  }

  {
    key_paths.emplace_back(u"foo");
    char expected[] = {0, 0,                      // Header
                       1,                         // Type is string
                       3, 0, 'f', 0, 'o', 0, 'o'  // String length 3, UTF-16BE
    };
    encoded_paths.push_back(
        std::string(expected, expected + std::size(expected)));
  }

  {
    key_paths.emplace_back(u"foo.bar");
    char expected[] = {0, 0,  // Header
                       1,     // Type is string
                       7, 0, 'f', 0, 'o', 0, 'o', 0, '.', 0, 'b', 0, 'a', 0,
                       'r'  // String length 7, UTF-16BE
    };
    encoded_paths.push_back(
        std::string(expected, expected + std::size(expected)));
  }

  {
    std::vector<std::u16string> array = {u"", u"foo", u"foo.bar"};

    key_paths.push_back(IndexedDBKeyPath(array));
    char expected[] = {0, 0,                       // Header
                       2, 3,                       // Type is array, length is 3
                       0,                          // Member 1 (String length 0)
                       3, 0, 'f', 0, 'o', 0, 'o',  // Member 2 (String length 3)
                       7, 0, 'f', 0, 'o', 0, 'o', 0, '.', 0, 'b', 0, 'a', 0,
                       'r'  // Member 3 (String length 7)
    };
    encoded_paths.push_back(
        std::string(expected, expected + std::size(expected)));
  }

  ASSERT_EQ(key_paths.size(), encoded_paths.size());
  for (size_t i = 0; i < key_paths.size(); ++i) {
    IndexedDBKeyPath key_path = key_paths[i];
    std::string encoded = encoded_paths[i];

    std::string v = WrappedEncodeIDBKeyPath(key_path);
    EXPECT_EQ(encoded, v);

    std::string_view slice(encoded);
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
    journals.push_back({{5, DatabaseMetaDataKey::kAllBlobsNumber}});
  }

  {  // A bunch of items
    journals.push_back(
        {{4, 7}, {5, 6}, {4, 5}, {4, 4}, {1, 12}, {4, 3}, {15, 14}});
  }

  for (const auto& journal_iter : journals) {
    std::string encoding;
    EncodeBlobJournal(journal_iter, &encoding);
    std::string_view slice(encoding);
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
    std::string_view slice(encoding);
    BlobJournalType journal_out;
    EXPECT_FALSE(DecodeBlobJournal(&slice, &journal_out));
  }
}

TEST(IndexedDBLevelDBCodingTest, DecodeLegacyIDBKeyPath) {
  // Legacy encoding of string key paths.
  std::vector<IndexedDBKeyPath> key_paths;
  std::vector<std::string> encoded_paths;

  {
    key_paths.push_back(IndexedDBKeyPath(u""));
    encoded_paths.push_back(std::string());
  }
  {
    key_paths.emplace_back(u"foo");
    char expected[] = {0, 'f', 0, 'o', 0, 'o'};
    encoded_paths.push_back(std::string(expected, std::size(expected)));
  }
  {
    key_paths.emplace_back(u"foo.bar");
    char expected[] = {0, 'f', 0, 'o', 0, 'o', 0, '.', 0, 'b', 0, 'a', 0, 'r'};
    encoded_paths.push_back(std::string(expected, std::size(expected)));
  }

  ASSERT_EQ(key_paths.size(), encoded_paths.size());
  for (size_t i = 0; i < key_paths.size(); ++i) {
    IndexedDBKeyPath key_path = key_paths[i];
    std::string encoded = encoded_paths[i];

    std::string_view slice(encoded);
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

      IndexedDBKey(u""),
      IndexedDBKey(u"a"),
      IndexedDBKey(u"b"),
      IndexedDBKey(u"baaa"),
      IndexedDBKey(u"baab"),
      IndexedDBKey(u"c"),

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
      CreateArrayIDBKey(IndexedDBKey(u"")),
      CreateArrayIDBKey(IndexedDBKey(u""), IndexedDBKey(u"a")),
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
    std::string_view slice;

    slice = std::string_view(encoded_a);
    EXPECT_TRUE(ExtractEncodedIDBKey(&slice, &extracted_a));
    EXPECT_TRUE(slice.empty());
    EXPECT_EQ(encoded_a, extracted_a);

    slice = std::string_view(encoded_b);
    EXPECT_TRUE(ExtractEncodedIDBKey(&slice, &extracted_b));
    EXPECT_TRUE(slice.empty());
    EXPECT_EQ(encoded_b, extracted_b);

    EXPECT_LT(CompareKeys(extracted_a, extracted_b), 0);
    EXPECT_GT(CompareKeys(extracted_b, extracted_a), 0);
    EXPECT_EQ(CompareKeys(extracted_a, extracted_a), 0);
    EXPECT_EQ(CompareKeys(extracted_b, extracted_b), 0);

    slice = std::string_view(&*encoded_a.begin(), encoded_a.size() - 1);
    EXPECT_FALSE(ExtractEncodedIDBKey(&slice, &extracted_a));
  }
}

TEST(IndexedDBLevelDBCodingTest, EncodeAndCompareIDBKeysWithSentinels) {
  std::vector<IndexedDBKey> keys = {
      IndexedDBKey(-15, blink::mojom::IDBKeyType::Number),
      IndexedDBKey(-10, blink::mojom::IDBKeyType::Number),
      IndexedDBKey(0, blink::mojom::IDBKeyType::Number),
      IndexedDBKey(3.14, blink::mojom::IDBKeyType::Number),
      IndexedDBKey(42, blink::mojom::IDBKeyType::Number),

      IndexedDBKey(0, blink::mojom::IDBKeyType::Date),
      IndexedDBKey(100, blink::mojom::IDBKeyType::Date),
      IndexedDBKey(100000, blink::mojom::IDBKeyType::Date),

      IndexedDBKey(u""),
      IndexedDBKey(u"a"),
      IndexedDBKey(u"b"),
      IndexedDBKey(u"baaa"),
      IndexedDBKey(u"baab"),
      IndexedDBKey(u"c"),

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
      CreateArrayIDBKey(IndexedDBKey(u"")),
      CreateArrayIDBKey(IndexedDBKey(u""), IndexedDBKey(u"a")),
      CreateArrayIDBKey(CreateArrayIDBKey()),
      CreateArrayIDBKey(CreateArrayIDBKey(), CreateArrayIDBKey()),
      CreateArrayIDBKey(CreateArrayIDBKey(CreateArrayIDBKey())),
      CreateArrayIDBKey(
          CreateArrayIDBKey(CreateArrayIDBKey(CreateArrayIDBKey()))),
  };

  for (size_t i = 0; i < keys.size() - 1; ++i) {
    const IndexedDBKey& key_a = keys[i];
    const IndexedDBKey& key_b = keys[i + 1];

    SCOPED_TRACE(testing::Message() << "Comparing keys " << key_a.DebugString()
                                    << " and " << key_b.DebugString());

    EXPECT_TRUE(key_a.IsLessThan(key_b));

    std::string encoded_a;
    EncodeSortableIDBKey(key_a, &encoded_a);
    EXPECT_TRUE(encoded_a.size());
    std::string encoded_b;
    EncodeSortableIDBKey(key_b, &encoded_b);
    EXPECT_TRUE(encoded_b.size());

    auto sqlite_compare = [](const std::string& a, const std::string& b) {
      return std::memcmp(a.c_str(), b.c_str(),
                         std::min(a.length(), b.length()));
    };

    EXPECT_LT(sqlite_compare(encoded_a, encoded_b), 0);
    EXPECT_GT(sqlite_compare(encoded_b, encoded_a), 0);
    EXPECT_EQ(sqlite_compare(encoded_a, encoded_a), 0);
    EXPECT_EQ(sqlite_compare(encoded_b, encoded_b), 0);
  }

  // Also test decoding by treating all test cases as one massive array key.
  const IndexedDBKey all_keys_key(keys);
  std::string encoded;
  EncodeSortableIDBKey(all_keys_key, &encoded);
  IndexedDBKey decoded_value;
  ASSERT_TRUE(DecodeSortableIDBKey(encoded, &decoded_value));
  EXPECT_TRUE(all_keys_key.Equals(decoded_value))
      << "Original is\n"
      << all_keys_key.DebugString() << "\nwhereas depickled version is\n"
      << decoded_value.DebugString();
}

TEST(IndexedDBLevelDBCodingTest, DecodeSortableWithCorruption) {
  std::vector<std::string> cases = {
      // Empty string.
      {},
      // Binary with bad meta-mark.
      {"\x40\x02\xff\x00", 4},
      // String with bad meta-mark.
      {"\x30\x00\x02\xff\xff\x00\x00", 7},
      // Array without terminating sentinel.
      {"\x50\x20\xff\xff\xff\xff", 6},
      // String with no terminating sentinel.
      {"\x30\x00\x01\xff\xff", 5},
      // Double with insufficient bytes.
      {"\x10\x00\x01\xff", 4},
  };

  for (const auto& test_case : cases) {
    blink::IndexedDBKey value;
    EXPECT_FALSE(DecodeSortableIDBKey(test_case, &value));
  }
}

// Verify that encoded doubles compare in the same order as C++ double
// arithmetic.
TEST(IndexedDBLevelDBCodingTest, EncodeSortableDoubles) {
  std::vector<double> values = {
      0.0,
      -0.0,
      1.0,
      -1.0,

      std::numeric_limits<double>::infinity(),
      -std::numeric_limits<double>::infinity(),
      std::numeric_limits<double>::lowest(),
      std::numeric_limits<double>::max(),

      std::numeric_limits<double>::min(),
      -std::numeric_limits<double>::min(),
      std::numeric_limits<double>::min() * 10,
      -std::numeric_limits<double>::min() * 10,

      std::numeric_limits<double>::denorm_min(),
      -std::numeric_limits<double>::denorm_min(),
      std::numeric_limits<double>::denorm_min() * 10,
      -std::numeric_limits<double>::denorm_min() * 10,
  };

  for (double value_a : values) {
    for (double value_b : values) {
      SCOPED_TRACE(testing::Message()
                   << "Comparing " << value_a << " and " << value_b);

      std::string encoded_a;
      EncodeSortableIDBKey(
          IndexedDBKey(value_a, blink::mojom::IDBKeyType::Number), &encoded_a);
      EXPECT_TRUE(encoded_a.size());
      std::string encoded_b;
      EncodeSortableIDBKey(
          IndexedDBKey(value_b, blink::mojom::IDBKeyType::Number), &encoded_b);
      EXPECT_TRUE(encoded_b.size());
      EXPECT_EQ(encoded_a.size(), encoded_b.size());

      auto sqlite_compare = [](const std::string& a, const std::string& b) {
        return std::memcmp(a.c_str(), b.c_str(),
                           std::min(a.length(), b.length()));
      };

      if (value_a < value_b) {
        EXPECT_LT(sqlite_compare(encoded_a, encoded_b), 0);
      } else if (value_a == value_b) {
        EXPECT_EQ(sqlite_compare(encoded_a, encoded_b), 0);
      } else {
        EXPECT_GT(sqlite_compare(encoded_a, encoded_b), 0);
      }
    }
  }

  for (double value : values) {
    const IndexedDBKey key(value, blink::mojom::IDBKeyType::Number);
    std::string encoded;
    EncodeSortableIDBKey(key, &encoded);
    IndexedDBKey decoded_value;
    ASSERT_TRUE(DecodeSortableIDBKey(encoded, &decoded_value));
    EXPECT_TRUE(key.Equals(decoded_value))
        << "Original is\n"
        << key.DebugString() << "\nwhereas depickled version is\n"
        << decoded_value.DebugString();
  }
}

TEST(IndexedDBLevelDBCodingTest, ComparisonTest) {
  std::vector<std::string> keys = {
      SchemaVersionKey::Encode(),
      MaxDatabaseIdKey::Encode(),
      DatabaseFreeListKey::Encode(0),
      DatabaseFreeListKey::EncodeMaxKey(),
      DatabaseNameKey::Encode("", u""),
      DatabaseNameKey::Encode("", u"a"),
      DatabaseNameKey::Encode("a", u"a"),

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
      ObjectStoreNamesKey::Encode(1, u""),
      ObjectStoreNamesKey::Encode(1, u"a"),
      IndexNamesKey::Encode(1, 1, u""),
      IndexNamesKey::Encode(1, 1, u"a"),
      IndexNamesKey::Encode(1, 2, u"a"),
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

      IndexDataKey::Encode(1, 1, 30, IndexedDBKey(u"user key"),
                           IndexedDBKey(u"primary key")),
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
    std::string_view piece(key);
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
}  // namespace content::indexed_db
