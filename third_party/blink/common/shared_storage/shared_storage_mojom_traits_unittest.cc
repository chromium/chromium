// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/shared_storage/shared_storage_mojom_traits.h"

#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/shared_storage/shared_storage.mojom.h"

namespace blink {
namespace {

// Divide the byte limit by two to get the character limit for a key or value.
constexpr int kMaxChar16StringLength = 5242880 / 2;

TEST(SharedStorageMojomTraitsTest, SerializeAndDeserializeKeyArgument) {
  std::u16string success_originals[] = {
      std::u16string(u"c"), std::u16string(u"hello world"),
      std::u16string(kMaxChar16StringLength, 'c')};
  for (auto& original : success_originals) {
    std::u16string copied;
    EXPECT_TRUE(
        mojo::test::SerializeAndDeserialize<mojom::SharedStorageKeyArgument>(
            original, copied));
    EXPECT_EQ(original, copied);
  }

  std::u16string failure_originals[] = {
      std::u16string(), std::u16string(kMaxChar16StringLength + 1, 'c')};
  for (auto& original : failure_originals) {
    std::u16string copied;
    EXPECT_FALSE(
        mojo::test::SerializeAndDeserialize<mojom::SharedStorageKeyArgument>(
            original, copied));
  }
}

TEST(SharedStorageMojomTraitsTest, SerializeAndDeserializeValueArgument) {
  std::u16string success_originals[] = {
      std::u16string(), std::u16string(u"c"), std::u16string(u"hello world"),
      std::u16string(kMaxChar16StringLength, 'c')};
  for (auto& original : success_originals) {
    std::u16string copied;
    EXPECT_TRUE(
        mojo::test::SerializeAndDeserialize<mojom::SharedStorageValueArgument>(
            original, copied));
    EXPECT_EQ(original, copied);
  }

  std::u16string failure_originals[] = {
      std::u16string(kMaxChar16StringLength + 1, 'c')};
  for (auto& original : failure_originals) {
    std::u16string copied;
    EXPECT_FALSE(
        mojo::test::SerializeAndDeserialize<mojom::SharedStorageValueArgument>(
            original, copied));
  }
}

}  // namespace
}  // namespace blink
