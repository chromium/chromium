// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/dml/tensor_desc.h"

#include <vector>

#include "base/numerics/safe_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/microsoft_dxheaders/include/directml.h"

namespace webnn::dml {

class WebNNTensorDescTest : public testing::Test {};

TEST_F(WebNNTensorDescTest, CreateAndCopyTensorDescA) {
  // Test creating and copying a TensorDesc with empty dimensions, and
  // whether its' members have been set valid values.
  TensorDesc tensor_a(DML_TENSOR_DATA_TYPE_FLOAT32, {});
  EXPECT_EQ(tensor_a.GetDataType(), DML_TENSOR_DATA_TYPE_FLOAT32);
  EXPECT_EQ(tensor_a.GetFlags(), DML_TENSOR_FLAG_NONE);
  EXPECT_EQ(tensor_a.GetDimensions(), std::vector<uint32_t>{1});
  EXPECT_EQ(tensor_a.GetStrides(), std::vector<uint32_t>{1});
  EXPECT_EQ(tensor_a.GetTotalTensorSizeInBytes(), 4u);

  TensorDesc tensor_a_copy1(tensor_a);
  EXPECT_EQ(tensor_a_copy1, tensor_a);

  TensorDesc tensor_a_copy2 = tensor_a;
  EXPECT_EQ(tensor_a_copy2, tensor_a);

  TensorDesc tensor_a_copy3(std::move(tensor_a_copy2));
  EXPECT_EQ(tensor_a_copy3, tensor_a);

  TensorDesc tensor_a_copy4 = std::move(tensor_a_copy3);
  EXPECT_EQ(tensor_a_copy4, tensor_a);

  DML_BUFFER_TENSOR_DESC& buffer_a_desc = tensor_a.buffer_desc_;
  EXPECT_EQ(tensor_a.tensor_desc_.Desc, &buffer_a_desc);
  EXPECT_EQ(buffer_a_desc.DimensionCount,
            base::checked_cast<uint32_t>(tensor_a.dimensions_.size()));
  EXPECT_EQ(buffer_a_desc.Sizes, tensor_a.dimensions_.data());
  EXPECT_EQ(buffer_a_desc.Strides, tensor_a.strides_.data());
}

TEST_F(WebNNTensorDescTest, CreateAndCopyTensorDescB) {
  // Test creating and copying a TensorDesc with DML_TENSOR_FLAG_OWNED_BY_DML
  // and dimensions, and whether its' members have been set valid values.
  std::vector<uint32_t> dimensions = {1, 2, 3}, strides = {6, 3, 1};
  TensorDesc tensor_b(DML_TENSOR_DATA_TYPE_FLOAT32,
                      DML_TENSOR_FLAG_OWNED_BY_DML, dimensions);
  EXPECT_EQ(tensor_b.GetDataType(), DML_TENSOR_DATA_TYPE_FLOAT32);
  EXPECT_EQ(tensor_b.GetFlags(), DML_TENSOR_FLAG_OWNED_BY_DML);
  EXPECT_EQ(tensor_b.GetDimensions(), dimensions);
  EXPECT_EQ(tensor_b.GetStrides(), strides);
  EXPECT_EQ(tensor_b.GetTotalTensorSizeInBytes(), 24u);

  TensorDesc tensor_b_copy1(tensor_b);
  EXPECT_EQ(tensor_b_copy1, tensor_b);

  TensorDesc tensor_b_copy2 = tensor_b;
  EXPECT_EQ(tensor_b_copy2, tensor_b);

  TensorDesc tensor_b_copy3(std::move(tensor_b_copy2));
  EXPECT_EQ(tensor_b_copy3, tensor_b);

  TensorDesc tensor_b_copy4 = std::move(tensor_b_copy3);
  EXPECT_EQ(tensor_b_copy4, tensor_b);

  DML_BUFFER_TENSOR_DESC& buffer_b_desc = tensor_b.buffer_desc_;
  EXPECT_EQ(tensor_b.tensor_desc_.Desc, &buffer_b_desc);
  EXPECT_EQ(buffer_b_desc.DimensionCount,
            base::checked_cast<uint32_t>(tensor_b.dimensions_.size()));
  EXPECT_EQ(buffer_b_desc.Sizes, tensor_b.dimensions_.data());
  EXPECT_EQ(buffer_b_desc.Strides, tensor_b.strides_.data());
}

TEST_F(WebNNTensorDescTest, CreateAndCopyTensorDescC) {
  // Test creating and copying a TensorDesc with strides and dimensions, and
  // whether its' members have been set valid values.
  std::vector<uint32_t> dimensions = {1, 2, 3}, strides = {6, 3, 1};
  TensorDesc tensor_c(DML_TENSOR_DATA_TYPE_FLOAT32,
                      DML_TENSOR_FLAG_OWNED_BY_DML, dimensions, strides);
  EXPECT_EQ(tensor_c.GetDataType(), DML_TENSOR_DATA_TYPE_FLOAT32);
  EXPECT_EQ(tensor_c.GetFlags(), DML_TENSOR_FLAG_OWNED_BY_DML);
  EXPECT_EQ(tensor_c.GetDimensions(), dimensions);
  EXPECT_EQ(tensor_c.GetStrides(), strides);
  EXPECT_EQ(tensor_c.GetTotalTensorSizeInBytes(), 24u);

  TensorDesc tensor_c_copy1(tensor_c);
  EXPECT_EQ(tensor_c_copy1, tensor_c);

  TensorDesc tensor_c_copy2 = tensor_c;
  EXPECT_EQ(tensor_c_copy2, tensor_c);

  TensorDesc tensor_c_copy3(std::move(tensor_c_copy2));
  EXPECT_EQ(tensor_c_copy3, tensor_c);

  TensorDesc tensor_c_copy4 = std::move(tensor_c_copy3);
  EXPECT_EQ(tensor_c_copy4, tensor_c);

  DML_BUFFER_TENSOR_DESC& buffer_c_desc = tensor_c.buffer_desc_;
  EXPECT_EQ(tensor_c.tensor_desc_.Desc, &buffer_c_desc);
  EXPECT_EQ(buffer_c_desc.DimensionCount,
            base::checked_cast<uint32_t>(tensor_c.dimensions_.size()));
  EXPECT_EQ(buffer_c_desc.Sizes, tensor_c.dimensions_.data());
  EXPECT_EQ(buffer_c_desc.Strides, tensor_c.strides_.data());
}

TEST_F(WebNNTensorDescTest, TransposeTensorDesc) {
  std::vector<uint32_t> dimensions = {1, 2, 3}, strides = {6, 3, 1};
  TensorDesc tensor(DML_TENSOR_DATA_TYPE_FLOAT32, DML_TENSOR_FLAG_OWNED_BY_DML,
                    dimensions, strides);
  EXPECT_EQ(tensor.GetDataType(), DML_TENSOR_DATA_TYPE_FLOAT32);
  EXPECT_EQ(tensor.GetFlags(), DML_TENSOR_FLAG_OWNED_BY_DML);
  EXPECT_EQ(tensor.GetDimensions(), dimensions);
  EXPECT_EQ(tensor.GetStrides(), strides);
  EXPECT_EQ(tensor.GetTotalTensorSizeInBytes(), 24u);

  std::vector<uint32_t> permutation = {2, 0, 1};
  tensor.Transpose(permutation);

  EXPECT_EQ(tensor.GetDimensions(), (std::vector<uint32_t>{3, 1, 2}));
  EXPECT_EQ(tensor.GetStrides(), (std::vector<uint32_t>{1, 6, 3}));
}

TEST_F(WebNNTensorDescTest, FlattenTensorDesc) {
  std::vector<uint32_t> dimensions = {2, 2, 2, 2, 3};
  TensorDesc tensor(DML_TENSOR_DATA_TYPE_FLOAT32, dimensions);
  EXPECT_TRUE(tensor.RightAlignedFlattenTo(4));
  EXPECT_EQ(tensor.GetDimensions(), (std::vector<uint32_t>{4, 2, 2, 3}));
  EXPECT_EQ(tensor.GetStrides(), (std::vector<uint32_t>{12, 6, 3, 1}));
}

TEST_F(WebNNTensorDescTest, TransposeAndFlattenTensorDesc) {
  {
    std::vector<uint32_t> dimensions = {2, 2, 1, 2, 3};
    TensorDesc tensor(DML_TENSOR_DATA_TYPE_FLOAT32, dimensions);
    tensor.Transpose(std::vector<uint32_t>{0, 1, 2, 4, 3});
    EXPECT_TRUE(tensor.RightAlignedFlattenTo(4));
    EXPECT_EQ(tensor.GetDimensions(), (std::vector<uint32_t>{4, 1, 3, 2}));
    EXPECT_EQ(tensor.GetStrides(), (std::vector<uint32_t>{6, 6, 1, 3}));
  }
  {
    std::vector<uint32_t> dimensions = {2, 2, 1, 2, 3};
    TensorDesc tensor(DML_TENSOR_DATA_TYPE_FLOAT32, dimensions);
    tensor.Transpose(std::vector<uint32_t>{1, 0, 2, 3, 4});
    EXPECT_TRUE(tensor.RightAlignedFlattenTo(3));
    EXPECT_EQ(tensor.GetDimensions(), (std::vector<uint32_t>{4, 2, 3}));
    EXPECT_EQ(tensor.GetStrides(), (std::vector<uint32_t>{6, 3, 1}));
  }
  // Test that some transposed tensor can't be flattened.
  {
    std::vector<uint32_t> dimensions = {5, 4, 3, 2, 1};
    TensorDesc tensor(DML_TENSOR_DATA_TYPE_FLOAT32, dimensions);
    tensor.Transpose(std::vector<uint32_t>{1, 0, 2, 3, 4});
    EXPECT_FALSE(tensor.RightAlignedFlattenTo(4));
    EXPECT_EQ(tensor.GetDimensions(), (std::vector<uint32_t>{4, 5, 3, 2, 1}));
    EXPECT_EQ(tensor.GetStrides(), (std::vector<uint32_t>{6, 24, 2, 1, 1}));
  }
}

TEST_F(WebNNTensorDescTest, BroadcastAndFlattenTensorDesc) {
  {
    std::vector<uint32_t> dimensions = {1, 1, 3};
    TensorDesc tensor(DML_TENSOR_DATA_TYPE_FLOAT32, dimensions);
    tensor.BroadcastTo(std::vector<uint32_t>{2, 2, 3});
    EXPECT_TRUE(tensor.RightAlignedFlattenTo(2));
    EXPECT_EQ(tensor.GetDimensions(), (std::vector<uint32_t>{4, 3}));
    EXPECT_EQ(tensor.GetStrides(), (std::vector<uint32_t>{0, 1}));
  }
  // Test that some broadcasted tensor can't be flattened.
  {
    std::vector<uint32_t> dimensions = {1, 2, 3};
    TensorDesc tensor(DML_TENSOR_DATA_TYPE_FLOAT32, dimensions);
    tensor.BroadcastTo(std::vector<uint32_t>{2, 2, 3});
    EXPECT_FALSE(tensor.RightAlignedFlattenTo(2));
    EXPECT_EQ(tensor.GetDimensions(), (std::vector<uint32_t>{2, 2, 3}));
    EXPECT_EQ(tensor.GetStrides(), (std::vector<uint32_t>{0, 3, 1}));
  }
}

TEST_F(WebNNTensorDescTest, CreateAndBroadcastTensorDesc) {
  // Test creating a TensorDesc with dimensions and broadcasting it.
  std::vector<uint32_t> dimensions = {1, 1, 3};
  TensorDesc tensor_desc(DML_TENSOR_DATA_TYPE_FLOAT32,
                         DML_TENSOR_FLAG_OWNED_BY_DML, dimensions);
  std::vector<uint32_t> broadcasted_dimensions = {1, 2, 3};
  tensor_desc.BroadcastTo(broadcasted_dimensions);
  EXPECT_EQ(tensor_desc.GetDimensions(), broadcasted_dimensions);
  EXPECT_EQ(tensor_desc.GetStrides(), (std::vector<uint32_t>{3, 0, 1}));
}

TEST_F(WebNNTensorDescTest, CreateTensorDescAndBroadcastTo0D) {
  // Test creating a TensorDesc with dimensions = {1} and broadcasting it to {}.
  std::vector<uint32_t> dimensions = {1};
  TensorDesc tensor_desc(DML_TENSOR_DATA_TYPE_FLOAT32,
                         DML_TENSOR_FLAG_OWNED_BY_DML, dimensions);
  std::vector<uint32_t> broadcasted_dimensions = {};
  tensor_desc.BroadcastTo(broadcasted_dimensions);
  EXPECT_EQ(tensor_desc.GetDimensions(), (std::vector<uint32_t>{1}));
  EXPECT_EQ(tensor_desc.GetStrides(), (std::vector<uint32_t>{1}));
}

TEST_F(WebNNTensorDescTest, Create0DTensorDescAndBroadcastTo0D) {
  // Test creating a TensorDesc with dimensions = {} and broadcasting it to {}.
  std::vector<uint32_t> dimensions = {};
  TensorDesc tensor_desc(DML_TENSOR_DATA_TYPE_FLOAT32,
                         DML_TENSOR_FLAG_OWNED_BY_DML, dimensions);
  std::vector<uint32_t> broadcasted_dimensions = {};
  tensor_desc.BroadcastTo(broadcasted_dimensions);
  EXPECT_EQ(tensor_desc.GetDimensions(), (std::vector<uint32_t>{1}));
  EXPECT_EQ(tensor_desc.GetStrides(), (std::vector<uint32_t>{1}));
}

TEST_F(WebNNTensorDescTest, EnsureMinimumRank) {
  // Test expanding the tensor from rank 3 to 5 in trailing alignment.
  std::vector<uint32_t> dimensions_a = {3, 4, 5};
  TensorDesc tensor_desc_a(DML_TENSOR_DATA_TYPE_INT32,
                           DML_TENSOR_FLAG_OWNED_BY_DML, dimensions_a);

  tensor_desc_a.EnsureMinimumRank(/*minimum_rank*/ 5,
                                  TensorDesc::Alignment::kTrailing);
  EXPECT_EQ(tensor_desc_a.GetDimensions(),
            (std::vector<uint32_t>{1, 1, 3, 4, 5}));
  EXPECT_EQ(tensor_desc_a.GetStrides(),
            (std::vector<uint32_t>{0, 0, 20, 5, 1}));

  // Test expanding the tensor from rank 4 to 8 in leading alignment.
  std::vector<uint32_t> dimensions_b = {1, 2, 3, 4};
  TensorDesc tensor_desc_b(DML_TENSOR_DATA_TYPE_FLOAT16,
                           DML_TENSOR_FLAG_OWNED_BY_DML, dimensions_b);

  tensor_desc_b.EnsureMinimumRank(/*minimum_rank*/ 8,
                                  TensorDesc::Alignment::kLeading);
  EXPECT_EQ(tensor_desc_b.GetDimensions(),
            (std::vector<uint32_t>{1, 2, 3, 4, 1, 1, 1, 1}));
  EXPECT_EQ(tensor_desc_b.GetStrides(),
            (std::vector<uint32_t>{24, 12, 4, 1, 0, 0, 0, 0}));
}

TEST_F(WebNNTensorDescTest, Make1DBroadcastCompatibleTo4D) {
  // Test creating a TensorDesc with dimensions = {2}, axes = {1} and
  // minimum_rank = 4.
  std::vector<uint32_t> dimensions = {2};
  TensorDesc tensor_desc(DML_TENSOR_DATA_TYPE_FLOAT32,
                         DML_TENSOR_FLAG_OWNED_BY_DML, std::move(dimensions));
  uint32_t axes[1] = {1};
  tensor_desc.MakeBroadcastCompatible(4, axes);
  EXPECT_EQ(tensor_desc.GetDimensions(), (std::vector<uint32_t>{1, 2, 1, 1}));
  EXPECT_EQ(tensor_desc.GetStrides(), (std::vector<uint32_t>{0, 1, 0, 0}));
}

TEST_F(WebNNTensorDescTest, Make2DBroadcastCompatibleTo4D) {
  // Test creating a TensorDesc with dimensions = {2, 3}, axes = {1, 3} and
  // minimum_rank = 4.
  std::vector<uint32_t> dimensions = {2, 3};
  TensorDesc tensor_desc(DML_TENSOR_DATA_TYPE_FLOAT32,
                         DML_TENSOR_FLAG_OWNED_BY_DML, std::move(dimensions));
  uint32_t axes[2] = {1, 3};
  tensor_desc.MakeBroadcastCompatible(4, axes);
  EXPECT_EQ(tensor_desc.GetDimensions(), (std::vector<uint32_t>{1, 2, 1, 3}));
  EXPECT_EQ(tensor_desc.GetStrides(), (std::vector<uint32_t>{0, 3, 0, 1}));
}

TEST_F(WebNNTensorDescTest, Make2DBroadcastCompatibleTo4DWithNoDefaultStrides) {
  // Test creating a TensorDesc with dimensions = {3, 2}, strides = {1, 3} axes
  // = {1, 3} and minimum_rank = 4.
  std::vector<uint32_t> dimensions = {3, 2};
  std::vector<uint32_t> strides = {1, 3};
  TensorDesc tensor_desc(DML_TENSOR_DATA_TYPE_FLOAT32,
                         DML_TENSOR_FLAG_OWNED_BY_DML, std::move(dimensions),
                         std::move(strides));
  uint32_t axes[2] = {1, 3};
  tensor_desc.MakeBroadcastCompatible(4, axes);
  EXPECT_EQ(tensor_desc.GetDimensions(), (std::vector<uint32_t>{1, 3, 1, 2}));
  EXPECT_EQ(tensor_desc.GetStrides(), (std::vector<uint32_t>{0, 1, 0, 3}));
}

TEST_F(WebNNTensorDescTest, Make0DBroadcastCompatibleTo4D) {
  // Test creating a scale TensorDesc, axes = {} and
  // minimum_rank = 4.
  TensorDesc tensor_desc(DML_TENSOR_DATA_TYPE_FLOAT32,
                         DML_TENSOR_FLAG_OWNED_BY_DML, {});
  tensor_desc.MakeBroadcastCompatible(4, {});
  EXPECT_EQ(tensor_desc.GetDimensions(), (std::vector<uint32_t>{1, 1, 1, 1}));
  EXPECT_EQ(tensor_desc.GetStrides(), (std::vector<uint32_t>{0, 0, 0, 0}));
}
}  // namespace webnn::dml
