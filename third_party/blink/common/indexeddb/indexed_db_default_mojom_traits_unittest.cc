// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/indexeddb/indexed_db_default_mojom_traits.h"

#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key_path.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"

namespace blink {
namespace {

TEST(IndexedDB, SerializeAndDeserializeValidKeypath) {
  IndexedDBKeyPath test_objects[] = {
      IndexedDBKeyPath(), IndexedDBKeyPath(u"id"), IndexedDBKeyPath(u"key.id"),
      IndexedDBKeyPath(std::vector<std::u16string>{u"id", u"key.id"})};

  for (auto& original : test_objects) {
    IndexedDBKeyPath copied;
    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::IDBKeyPath>(original,
                                                                       copied));
    EXPECT_EQ(original, copied);
  }
}

TEST(IndexedDB, CannotDeserializeInvalidKeypath) {
  IndexedDBKeyPath test_objects[] = {
      IndexedDBKeyPath(u"space "), IndexedDBKeyPath(u" space"),
      IndexedDBKeyPath(std::vector<std::u16string>{u"valid", u"with space"})};

  for (auto& original : test_objects) {
    IndexedDBKeyPath copied;
    EXPECT_FALSE(mojo::test::SerializeAndDeserialize<mojom::IDBKeyPath>(
        original, copied));
  }
}

}  // namespace
}  // namespace blink
