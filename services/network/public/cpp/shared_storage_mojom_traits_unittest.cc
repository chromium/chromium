// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/shared_storage_mojom_traits.h"

#include "base/test/scoped_feature_list.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/shared_storage_utils.h"
#include "services/network/public/mojom/shared_storage.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

// Divide the byte limit by two to get the character limit for a key or value.
constexpr int kMaxChar16StringLength = kMaxSharedStorageBytesPerOrigin / 2;

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

TEST(SharedStorageMojomTraitsTest, SerializeAndDeserializeLockName) {
  std::string success_originals[] = {std::string(), std::string("a"),
                                     std::string("abc")};
  for (auto& original : success_originals) {
    std::string copied;
    EXPECT_TRUE(
        mojo::test::SerializeAndDeserialize<mojom::LockName>(original, copied));
    EXPECT_EQ(original, copied);
  }

  std::string failure_originals[] = {std::string("-"), std::string("-a")};
  for (auto& original : failure_originals) {
    std::string copied;
    EXPECT_FALSE(
        mojo::test::SerializeAndDeserialize<mojom::LockName>(original, copied));
  }
}

TEST(
    SharedStorageMojomTraitsTest,
    SerializeAndDeserializeBatchUpdateMethodsArgument_LegacyBatchUpdate_HasInnerMethodLock_Success) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      network::features::kSharedStorageTransactionalBatchUpdate);

  auto method1 = mojom::SharedStorageModifierMethodWithOptions::New(
      mojom::SharedStorageModifierMethod::NewSetMethod(
          mojom::SharedStorageSetMethod::New(/*key=*/u"a", /*key=*/u"b",
                                             /*ignore_if_present=*/true)),
      /*with_lock=*/"lock1");

  auto method2 = mojom::SharedStorageModifierMethodWithOptions::New(
      mojom::SharedStorageModifierMethod::NewAppendMethod(
          mojom::SharedStorageAppendMethod::New(/*key=*/u"c", /*key=*/u"d")),
      /*with_lock=*/std::nullopt);

  std::vector<mojom::SharedStorageModifierMethodWithOptionsPtr>
      original_methods;
  original_methods.push_back(std::move(method1));
  original_methods.push_back(std::move(method2));

  std::vector<mojom::SharedStorageModifierMethodWithOptionsPtr> copied_methods;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
              mojom::SharedStorageBatchUpdateMethodsArgument>(original_methods,
                                                              copied_methods));
  EXPECT_EQ(original_methods, copied_methods);
}

TEST(
    SharedStorageMojomTraitsTest,
    SerializeAndDeserializeBatchUpdateMethodsArgument_TransactionalBatchUpdate_NoInnerMethodLock_Success) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      network::features::kSharedStorageTransactionalBatchUpdate);

  auto method1 = mojom::SharedStorageModifierMethodWithOptions::New(
      mojom::SharedStorageModifierMethod::NewSetMethod(
          mojom::SharedStorageSetMethod::New(/*key=*/u"a", /*key=*/u"b",
                                             /*ignore_if_present=*/true)),
      /*with_lock=*/std::nullopt);

  auto method2 = mojom::SharedStorageModifierMethodWithOptions::New(
      mojom::SharedStorageModifierMethod::NewAppendMethod(
          mojom::SharedStorageAppendMethod::New(/*key=*/u"c", /*key=*/u"d")),
      /*with_lock=*/std::nullopt);

  std::vector<mojom::SharedStorageModifierMethodWithOptionsPtr>
      original_methods;
  original_methods.push_back(std::move(method1));
  original_methods.push_back(std::move(method2));

  std::vector<mojom::SharedStorageModifierMethodWithOptionsPtr> copied_methods;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
              mojom::SharedStorageBatchUpdateMethodsArgument>(original_methods,
                                                              copied_methods));
  EXPECT_EQ(original_methods, copied_methods);
}

TEST(
    SharedStorageMojomTraitsTest,
    SerializeAndDeserializeBatchUpdateMethodsArgument_TransactionalBatchUpdate_HasInnerMethodLock_Failure) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      network::features::kSharedStorageTransactionalBatchUpdate);

  auto method1 = mojom::SharedStorageModifierMethodWithOptions::New(
      mojom::SharedStorageModifierMethod::NewSetMethod(
          mojom::SharedStorageSetMethod::New(/*key=*/u"a", /*key=*/u"b",
                                             /*ignore_if_present=*/true)),
      /*with_lock=*/"lock1");

  auto method2 = mojom::SharedStorageModifierMethodWithOptions::New(
      mojom::SharedStorageModifierMethod::NewAppendMethod(
          mojom::SharedStorageAppendMethod::New(/*key=*/u"c", /*key=*/u"d")),
      /*with_lock=*/std::nullopt);

  std::vector<mojom::SharedStorageModifierMethodWithOptionsPtr>
      original_methods;
  original_methods.push_back(std::move(method1));
  original_methods.push_back(std::move(method2));

  std::vector<mojom::SharedStorageModifierMethodWithOptionsPtr> copied_methods;
  EXPECT_FALSE(mojo::test::SerializeAndDeserialize<
               mojom::SharedStorageBatchUpdateMethodsArgument>(original_methods,
                                                               copied_methods));
}

}  // namespace
}  // namespace network
