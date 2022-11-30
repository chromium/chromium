// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"

#include <stddef.h>

#include <string>
#include <utility>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

TEST(IndexedDBKeyTest, KeySizeEstimates) {
  std::vector<IndexedDBKey> keys;
  std::vector<size_t> estimates;

  keys.push_back(IndexedDBKey());
  estimates.push_back(16u);  // Overhead.

  keys.push_back(IndexedDBKey(mojom::IDBKeyType::None));
  estimates.push_back(16u);

  double number = 3.14159;
  keys.push_back(IndexedDBKey(number, mojom::IDBKeyType::Number));
  estimates.push_back(24u);  // Overhead + sizeof(double).

  double date = 1370884329.0;
  keys.push_back(IndexedDBKey(date, mojom::IDBKeyType::Date));
  estimates.push_back(24u);  // Overhead + sizeof(double).

  const std::u16string string(1024, u'X');
  keys.push_back(IndexedDBKey(std::move(string)));
  // Overhead + string length * sizeof(char16_t).
  estimates.push_back(2064u);

  const size_t array_size = 1024;
  IndexedDBKey::KeyArray array;
  double value = 123.456;
  for (size_t i = 0; i < array_size; ++i) {
    array.push_back(IndexedDBKey(value, mojom::IDBKeyType::Number));
  }
  keys.push_back(IndexedDBKey(std::move(array)));
  // Overhead + array length * (Overhead + sizeof(double)).
  estimates.push_back(24592u);

  ASSERT_EQ(keys.size(), estimates.size());
  for (size_t i = 0; i < keys.size(); ++i) {
    EXPECT_EQ(estimates[i], keys[i].size_estimate());
  }
}

}  // namespace

}  // namespace blink
