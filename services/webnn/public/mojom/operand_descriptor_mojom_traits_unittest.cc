// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/public/mojom/operand_descriptor_mojom_traits.h"

#include <array>

#include "base/test/gtest_util.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/webnn/public/cpp/ml_tensor_usage.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/mojom/tensor_usage_mojom_traits.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/webnn_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {

namespace {

webnn::OperandDescriptor CreateInvalidOperandDescriptor() {
  return webnn::OperandDescriptor::UnsafeCreateForTesting(
      webnn::OperandDataType::kFloat32, std::array<uint32_t, 1>{0});
}

}  // namespace

TEST(OperandDescriptorMojomTraitsTest, Basic) {
  auto input = webnn::OperandDescriptor::CreateForDeserialization(
      webnn::OperandDataType::kInt32, std::array<uint32_t, 2>{2, 3}, {});
  ASSERT_TRUE(input.has_value());

  webnn::OperandDescriptor output = CreateInvalidOperandDescriptor();
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<webnn::mojom::OperandDescriptor>(
          *input, output));
  EXPECT_EQ(input, output);
}

TEST(OperandDescriptorMojomTraitsTest, Int4) {
  auto input = webnn::OperandDescriptor::CreateForDeserialization(
      webnn::OperandDataType::kInt4, std::array<uint32_t, 2>{3, 3}, {});
  ASSERT_TRUE(input.has_value());

  webnn::OperandDescriptor output = CreateInvalidOperandDescriptor();
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<webnn::mojom::OperandDescriptor>(
          *input, output));
  EXPECT_EQ(*input, output);
  // 9 int4 elements are packed in 5 bytes.
  EXPECT_EQ(output.NumberOfElements(), 9u);
  EXPECT_EQ(output.PackedByteLength(), 5u);
}

TEST(OperandDescriptorMojomTraitsTest, EmptyShape) {
  // Descriptors with an empty shape are treated as scalars.
  auto input = webnn::OperandDescriptor::CreateForDeserialization(
      webnn::OperandDataType::kInt32, {}, {});
  ASSERT_TRUE(input.has_value());

  webnn::OperandDescriptor output = CreateInvalidOperandDescriptor();
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<webnn::mojom::OperandDescriptor>(
          *input, output));
  EXPECT_EQ(*input, output);
  EXPECT_EQ(output.NumberOfElements(), 1u);
  EXPECT_EQ(output.PackedByteLength(), 4u);  // int32 is 4 bytes.
}

TEST(OperandDescriptorMojomTraitsTest, ZeroDimension) {
  // Descriptors with a zero-length dimension are not supported.
  const std::array<uint32_t, 3> shape{2, 0, 3};

  ASSERT_FALSE(webnn::OperandDescriptor::CreateForDeserialization(
                   webnn::OperandDataType::kInt32, shape, {})
                   .has_value());

  auto input = webnn::OperandDescriptor::UnsafeCreateForTesting(
      webnn::OperandDataType::kInt32, shape);
  webnn::OperandDescriptor output = CreateInvalidOperandDescriptor();
  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<webnn::mojom::OperandDescriptor>(
          input, output));
}

TEST(OperandDescriptorMojomTraitsTest, NumberOfElementsTooLarge) {
  // The number of elements overflows the max size_t on all platforms.
  const std::array<uint32_t, 3> shape{2, std::numeric_limits<uint32_t>::max(),
                                      std::numeric_limits<uint32_t>::max()};

  // Using int4 so that the byte length won't overflow the max size_t on 64-bit
  // platforms.
  ASSERT_FALSE(webnn::OperandDescriptor::CreateForDeserialization(
                   webnn::OperandDataType::kInt4, shape, {})
                   .has_value());

  auto input = webnn::OperandDescriptor::UnsafeCreateForTesting(
      webnn::OperandDataType::kInt4, shape);
  webnn::OperandDescriptor output = CreateInvalidOperandDescriptor();
  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<webnn::mojom::OperandDescriptor>(
          input, output));
}

TEST(OperandDescriptorMojomTraitsTest, ByteLengthTooLarge) {
  // The number of elements won't overflow the max size_t on 64-bit platforms.
  const std::array<uint32_t, 2> shape{std::numeric_limits<uint32_t>::max() / 2,
                                      std::numeric_limits<uint32_t>::max()};

  // The byte length overflows the max size_t on all platforms.
  ASSERT_FALSE(webnn::OperandDescriptor::CreateForDeserialization(
                   webnn::OperandDataType::kInt64, shape, {})
                   .has_value());

  auto input = webnn::OperandDescriptor::UnsafeCreateForTesting(
      webnn::OperandDataType::kInt64, shape);
  webnn::OperandDescriptor output = CreateInvalidOperandDescriptor();
  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<webnn::mojom::OperandDescriptor>(
          input, output));
}

TEST(OperandDescriptorMojomTraitsTest, ByteLengthExceedTensorSizeLimit) {
  // The large tensor out of byte length limit can be created, serialized and
  // deserialized with `OperandDescriptor::CreateForDeserialization` but not
  // `OperandDescriptor::Create`
  const std::array<uint32_t, 2> shape = {
      base::checked_cast<uint32_t>(std::numeric_limits<int32_t>::max() / 4), 2};

  auto input = webnn::OperandDescriptor::CreateForDeserialization(
      webnn::OperandDataType::kInt32, shape, {});
  ASSERT_TRUE(input.has_value());

  webnn::OperandDescriptor output = CreateInvalidOperandDescriptor();
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<webnn::mojom::OperandDescriptor>(
          *input, output));
  EXPECT_EQ(input, output);

  ASSERT_FALSE(webnn::OperandDescriptor::Create(
                   webnn::GetContextPropertiesForTesting(),
                   webnn::OperandDataType::kInt32, shape, /*label=*/"clamp")
                   .has_value());
}

TEST(OperandDescriptorMojomTraitsTest, DataType) {
  auto input = webnn::OperandDataType::kUint32;
  auto output = webnn::OperandDataType::kMinValue;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<webnn::mojom::DataType>(
      input, output));
}

TEST(OperandDescriptorMojomTraitsTest, InvalidSizePendingPermutation) {
  const std::array<uint32_t, 2> shape{1u, 2u};

  const std::array<uint32_t, 1> pending_permutation{1};
  ASSERT_FALSE(webnn::OperandDescriptor::CreateForDeserialization(
                   webnn::OperandDataType::kInt64, shape, pending_permutation)
                   .has_value());

  auto input = webnn::OperandDescriptor::UnsafeCreateForTesting(
      webnn::OperandDataType::kInt64, shape, pending_permutation);
  webnn::OperandDescriptor output = CreateInvalidOperandDescriptor();
  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<webnn::mojom::OperandDescriptor>(
          input, output));
}

TEST(OperandDescriptorMojomTraitsTest, InvalidDimensionPendingPermutation) {
  const std::array<uint32_t, 2> shape{1u, 2u};

  const std::array<uint32_t, 2> pending_permutation{3, 2};
  ASSERT_FALSE(webnn::OperandDescriptor::CreateForDeserialization(
                   webnn::OperandDataType::kInt64, shape, pending_permutation)
                   .has_value());

  auto input = webnn::OperandDescriptor::UnsafeCreateForTesting(
      webnn::OperandDataType::kInt64, shape, pending_permutation);
  webnn::OperandDescriptor output = CreateInvalidOperandDescriptor();
  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<webnn::mojom::OperandDescriptor>(
          input, output));
}

TEST(OperandDescriptorMojomTraitsTest, SubBytePendingPermutation) {
  const std::array<uint32_t, 2> shape{1u, 2u};

  const std::array<uint32_t, 2> pending_permutation{1, 0};
  ASSERT_FALSE(webnn::OperandDescriptor::CreateForDeserialization(
                   webnn::OperandDataType::kInt4, shape, pending_permutation)
                   .has_value());

  auto input = webnn::OperandDescriptor::UnsafeCreateForTesting(
      webnn::OperandDataType::kInt4, shape, pending_permutation);
  webnn::OperandDescriptor output = CreateInvalidOperandDescriptor();
  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<webnn::mojom::OperandDescriptor>(
          input, output));
}

}  // namespace mojo
