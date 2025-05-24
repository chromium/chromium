// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/mojom/extension_id_mojom_traits.h"

#include <utility>

#include "extensions/common/extension_id.h"
#include "extensions/common/mojom/extension_id.mojom.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

TEST(ExtensionIdMojomTraitsTest, ValidId) {
  mojo::InlinedStructPtr<mojom::ExtensionId> input(std::in_place);
  input->id = ExtensionId("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");

  ExtensionId output;
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::ExtensionId>(input, output));

  EXPECT_EQ(input->id, output);
}

class InvalidExtensionIdMojomTraitsTest
    : public testing::Test,
      public testing::WithParamInterface<ExtensionId> {
 public:
  InvalidExtensionIdMojomTraitsTest() = default;
  InvalidExtensionIdMojomTraitsTest(const InvalidExtensionIdMojomTraitsTest&) =
      delete;
  InvalidExtensionIdMojomTraitsTest& operator=(
      const InvalidExtensionIdMojomTraitsTest&) = delete;
};

TEST_P(InvalidExtensionIdMojomTraitsTest, InvalidIds) {
  mojo::InlinedStructPtr<mojom::ExtensionId> input(std::in_place);
  input->id = GetParam();

  ExtensionId output;
  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<mojom::ExtensionId>(input, output));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    InvalidExtensionIdMojomTraitsTest,
    testing::Values(
        ExtensionId(),                                     // Empty id
        ExtensionId("a"),                                  // Too short
        ExtensionId("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),  // Too long
        ExtensionId("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa1")    // Invalid characters
        ));

}  // namespace

}  // namespace extensions
