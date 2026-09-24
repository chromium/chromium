// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/idb_key.h"

#include <memory>
#include <utility>

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

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

}  // namespace
}  // namespace blink
