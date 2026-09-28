// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/idb_key.h"

#include <memory>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-blink.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {
namespace {

scoped_refptr<base::RefCountedData<Vector<char>>> MakeBinaryData(
    std::initializer_list<char> bytes) {
  auto data =
      base::MakeRefCounted<base::RefCountedData<Vector<char>>>(Vector<char>());
  data->data.Append(bytes.begin(), static_cast<wtf_size_t>(bytes.size()));
  return data;
}

TEST(IDBKeyTest, ToMultiEntryArray) {
  IDBKey::KeyArray array;
  array.push_back(IDBKey::CreateNumber(3.0));
  array.push_back(IDBKey::CreateNumber(1.0));
  array.push_back(IDBKey::CreateInvalid());
  array.push_back(IDBKey::CreateNumber(3.0));  // Duplicate
  array.push_back(IDBKey::CreateNumber(2.0));

  auto multi_entry =
      IDBKey::ToMultiEntryArray(IDBKey::CreateArray(std::move(array)));

  // Invalid key removed, duplicates removed, and sorted: [1.0, 2.0, 3.0]
  ASSERT_EQ(multi_entry.size(), 3u);
  EXPECT_DOUBLE_EQ(multi_entry[0]->Number(), 1.0);
  EXPECT_DOUBLE_EQ(multi_entry[1]->Number(), 2.0);
  EXPECT_DOUBLE_EQ(multi_entry[2]->Number(), 3.0);
}

TEST(IDBKeyTest, TypesAndAccessors) {
  {
    auto invalid_key = IDBKey::CreateInvalid();
    EXPECT_EQ(invalid_key->GetType(), mojom::IDBKeyType::Invalid);
    EXPECT_FALSE(invalid_key->IsValid());
    EXPECT_EQ(invalid_key->SizeEstimate(), 16u);
  }
  {
    auto none_key = IDBKey::CreateNone();
    EXPECT_EQ(none_key->GetType(), mojom::IDBKeyType::None);
    EXPECT_TRUE(none_key->IsValid());
    EXPECT_EQ(none_key->SizeEstimate(), 16u);
  }
  {
    auto number_key = IDBKey::CreateNumber(42.5);
    EXPECT_EQ(number_key->GetType(), mojom::IDBKeyType::Number);
    EXPECT_TRUE(number_key->IsValid());
    EXPECT_DOUBLE_EQ(number_key->Number(), 42.5);
    EXPECT_EQ(number_key->SizeEstimate(), 16u + sizeof(double));
  }
  {
    auto date_key = IDBKey::CreateDate(123456789.0);
    EXPECT_EQ(date_key->GetType(), mojom::IDBKeyType::Date);
    EXPECT_TRUE(date_key->IsValid());
    EXPECT_DOUBLE_EQ(date_key->Date(), 123456789.0);
    EXPECT_EQ(date_key->SizeEstimate(), 16u + sizeof(double));
  }
  {
    String str = "test string";
    auto string_key = IDBKey::CreateString(str);
    EXPECT_EQ(string_key->GetType(), mojom::IDBKeyType::String);
    EXPECT_TRUE(string_key->IsValid());
    EXPECT_EQ(string_key->GetString(), str);
    EXPECT_EQ(string_key->SizeEstimate(), 16u + (str.length() * sizeof(UChar)));
  }
  {
    auto bin_data = MakeBinaryData({1, 2, 3, 4});
    auto binary_key = IDBKey::CreateBinary(bin_data);
    EXPECT_EQ(binary_key->GetType(), mojom::IDBKeyType::Binary);
    EXPECT_TRUE(binary_key->IsValid());
    EXPECT_EQ(binary_key->Binary()->data.size(), 4u);
    EXPECT_EQ(binary_key->SizeEstimate(), 16u + 4u);
  }
  {
    IDBKey::KeyArray array;
    array.push_back(IDBKey::CreateNumber(1.0));
    array.push_back(IDBKey::CreateString("two"));
    auto array_key = IDBKey::CreateArray(std::move(array));
    EXPECT_EQ(array_key->GetType(), mojom::IDBKeyType::Array);
    EXPECT_TRUE(array_key->IsValid());
    EXPECT_EQ(array_key->Array().size(), 2u);
    EXPECT_EQ(array_key->SizeEstimate(),
              16u + (16u + sizeof(double)) + (16u + (3 * sizeof(UChar))));
  }
}

TEST(IDBKeyTest, ArrayValidity) {
  // An array with valid elements is valid.
  {
    IDBKey::KeyArray array;
    array.push_back(IDBKey::CreateNumber(1.0));
    array.push_back(IDBKey::CreateDate(100.0));
    auto key = IDBKey::CreateArray(std::move(array));
    EXPECT_TRUE(key->IsValid());
  }
  // An array containing an invalid key is invalid.
  {
    IDBKey::KeyArray array;
    array.push_back(IDBKey::CreateNumber(1.0));
    array.push_back(IDBKey::CreateInvalid());
    auto key = IDBKey::CreateArray(std::move(array));
    EXPECT_FALSE(key->IsValid());
  }
}

TEST(IDBKeyTest, ComparisonBetweenTypes) {
  // Type sort order per IndexedDB specification:
  // Number < Date < String < Binary < Array
  auto number_key = IDBKey::CreateNumber(100.0);
  auto date_key = IDBKey::CreateDate(100.0);
  auto string_key = IDBKey::CreateString("100");
  auto binary_key = IDBKey::CreateBinary(MakeBinaryData({100}));
  IDBKey::KeyArray array;
  array.push_back(IDBKey::CreateNumber(100.0));
  auto array_key = IDBKey::CreateArray(std::move(array));

  EXPECT_TRUE(number_key->IsLessThan(date_key.get()));
  EXPECT_TRUE(date_key->IsLessThan(string_key.get()));
  EXPECT_TRUE(string_key->IsLessThan(binary_key.get()));
  EXPECT_TRUE(binary_key->IsLessThan(array_key.get()));

  EXPECT_FALSE(date_key->IsLessThan(number_key.get()));
  EXPECT_FALSE(string_key->IsLessThan(date_key.get()));
  EXPECT_FALSE(binary_key->IsLessThan(string_key.get()));
  EXPECT_FALSE(array_key->IsLessThan(binary_key.get()));
}

TEST(IDBKeyTest, ComparisonWithinTypes) {
  // Numbers
  {
    auto k1 = IDBKey::CreateNumber(-1.0);
    auto k2 = IDBKey::CreateNumber(0.0);
    auto k3 = IDBKey::CreateNumber(0.0);
    EXPECT_TRUE(k1->IsLessThan(k2.get()));
    EXPECT_TRUE(k2->IsEqual(k3.get()));
    EXPECT_FALSE(k2->IsLessThan(k1.get()));
  }
  // Dates
  {
    auto k1 = IDBKey::CreateDate(100.0);
    auto k2 = IDBKey::CreateDate(200.0);
    auto k3 = IDBKey::CreateDate(200.0);
    EXPECT_TRUE(k1->IsLessThan(k2.get()));
    EXPECT_TRUE(k2->IsEqual(k3.get()));
    EXPECT_FALSE(k2->IsLessThan(k1.get()));
  }
  // Strings
  {
    auto k1 = IDBKey::CreateString("abc");
    auto k2 = IDBKey::CreateString("abd");
    auto k3 = IDBKey::CreateString("abc");
    EXPECT_TRUE(k1->IsLessThan(k2.get()));
    EXPECT_TRUE(k1->IsEqual(k3.get()));
  }
  // Binary
  {
    auto k1 = IDBKey::CreateBinary(MakeBinaryData({1, 2}));
    auto k2 = IDBKey::CreateBinary(MakeBinaryData({1, 3}));
    auto k3 = IDBKey::CreateBinary(MakeBinaryData({1, 2, 0}));
    auto k4 = IDBKey::CreateBinary(MakeBinaryData({1, 2}));
    EXPECT_TRUE(k1->IsLessThan(k2.get()));
    EXPECT_TRUE(k1->IsLessThan(k3.get()));
    EXPECT_TRUE(k1->IsEqual(k4.get()));
  }
  // Arrays
  {
    IDBKey::KeyArray a1;
    a1.push_back(IDBKey::CreateNumber(1.0));
    a1.push_back(IDBKey::CreateNumber(2.0));

    IDBKey::KeyArray a2;
    a2.push_back(IDBKey::CreateNumber(1.0));
    a2.push_back(IDBKey::CreateNumber(3.0));

    IDBKey::KeyArray a3;
    a3.push_back(IDBKey::CreateNumber(1.0));

    auto k1 = IDBKey::CreateArray(std::move(a1));
    auto k2 = IDBKey::CreateArray(std::move(a2));
    auto k3 = IDBKey::CreateArray(std::move(a3));

    EXPECT_TRUE(k1->IsLessThan(k2.get()));
    EXPECT_TRUE(k3->IsLessThan(k1.get()));
  }
}

TEST(IDBKeyTest, Clone) {
  {
    auto original = IDBKey::CreateInvalid();
    auto clone = IDBKey::Clone(original.get());
    EXPECT_EQ(clone->GetType(), mojom::IDBKeyType::Invalid);
  }
  {
    auto original = IDBKey::CreateNone();
    auto clone = IDBKey::Clone(original.get());
    EXPECT_EQ(clone->GetType(), mojom::IDBKeyType::None);
  }
  {
    auto original = IDBKey::CreateNumber(3.14);
    auto clone = IDBKey::Clone(original.get());
    EXPECT_TRUE(original->IsEqual(clone.get()));
  }
  {
    auto original = IDBKey::CreateDate(12345.0);
    auto clone = IDBKey::Clone(original.get());
    EXPECT_TRUE(original->IsEqual(clone.get()));
  }
  {
    auto original = IDBKey::CreateString("hello world");
    auto clone = IDBKey::Clone(original.get());
    EXPECT_TRUE(original->IsEqual(clone.get()));
  }
  {
    auto original = IDBKey::CreateBinary(MakeBinaryData({1, 2, 3}));
    auto clone = IDBKey::Clone(original.get());
    EXPECT_TRUE(original->IsEqual(clone.get()));
  }
  {
    IDBKey::KeyArray array;
    array.push_back(IDBKey::CreateNumber(1.0));
    array.push_back(IDBKey::CreateString("two"));
    auto original = IDBKey::CreateArray(std::move(array));
    auto clone = IDBKey::Clone(original.get());
    EXPECT_TRUE(original->IsEqual(clone.get()));
  }
}

}  // namespace
}  // namespace blink
