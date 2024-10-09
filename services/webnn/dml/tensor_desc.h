// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_DML_TENSOR_DESC_H_
#define SERVICES_WEBNN_DML_TENSOR_DESC_H_

#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/gtest_prod_util.h"
#include "third_party/microsoft_dxheaders/include/directml.h"

// Windows SDK headers should be included after DirectX headers.
#include <wrl.h>

namespace webnn::dml {

// The TensorDesc wraps a tensor description (DML_TENSOR_DESC) needed by a DML
// graph. It owns the tensor's dimensions, strides and DML_BUFFER_TENSOR_DESC.
// The TensorDesc is prepared for building a DML graph's description
// (DML_GRAPH_DESC).
class COMPONENT_EXPORT(WEBNN_SERVICE) TensorDesc final {
 public:
  enum class Alignment {
    // Align the elements to leading/left edge.
    kLeading,
    // Align the elements to trailing/right edge.
    kTrailing,
  };

  TensorDesc();
  TensorDesc(DML_TENSOR_DATA_TYPE data_type, std::vector<uint32_t> dimensions);
  TensorDesc(DML_TENSOR_DATA_TYPE data_type,
             DML_TENSOR_FLAGS flags,
             std::vector<uint32_t> dimensions,
             std::vector<uint32_t> strides = {});

  TensorDesc(const TensorDesc& other);
  TensorDesc(TensorDesc&& other);
  TensorDesc& operator=(const TensorDesc& other);
  TensorDesc& operator=(TensorDesc&& other);

  ~TensorDesc();

  DML_TENSOR_DATA_TYPE GetDataType() const { return buffer_desc_.DataType; }
  DML_TENSOR_FLAGS GetFlags() const { return buffer_desc_.Flags; }
  const std::vector<uint32_t>& GetDimensions() const { return dimensions_; }
  const std::vector<uint32_t>& GetStrides() const { return strides_; }
  uint64_t GetTotalTensorSizeInBytes() const {
    return buffer_desc_.TotalTensorSizeInBytes;
  }
  const DML_TENSOR_DESC& GetDMLTensorDesc() const { return tensor_desc_; }

  bool operator==(const TensorDesc& other) const;

  // Transpose the tensor by permuting the dimensions and strides following the
  // given permutation.
  void Transpose(base::span<const uint32_t> permutation);

  // Change the dimensions and strides of the TensorDesc according to the target
  // broadcasted dimensions and ignorable tails count.
  void BroadcastTo(base::span<const uint32_t> broadcasted_dims,
                   size_t ignorable_tails_count = 0);

  // Flatten the tensor from the leftmost dimensions to reduce its rank,
  // aligning the right dimensions. The strides of the TensorDesc will be
  // adjusted accordingly. The flattened rank must be greater than zero and not
  // exceed the original rank.
  // For example, a 5D tensor descriptor with dimensions [3, 4, 5, 6, 7] can be
  // flattened to a 4D tensor descriptor with dimensions [12, 5, 6, 7] by
  // folding the two leftmost dimensions [3, 4] to [12].
  // Some tensor descriptors with non-default strides cannot be flattened.
  // For example, if a tensor descriptor with dimensions [1, 2, 3] is
  // broadcasted to [2, 2, 3], the resulting tensor descriptor cannot be
  // flattened to 2D. Similarly, if a tensor descriptor with dimensions [5, 4,
  // 3, 2, 1] is transposed to [4, 5, 3, 2, 1], the resulting tensor descriptor
  // cannot be flattened to 4D.
  // Appending an identity operator to consume the non-default strides in
  // advance can ensure successful flattening.
  bool RightAlignedFlattenTo(size_t flattened_rank);

  // Add leading or trailing ones to dimensions and zeros to strides to ensure
  // the tensor rank >= minimum_rank.
  void EnsureMinimumRank(size_t minimum_rank, Alignment alignment);

  // Promotes the tensor shape to a compatible higher rank given a list of
  // target axes. e.g. given an existing shape of [2, 3], new rank of 4, and
  // target axes of [0, 3], the new shape would be [2, 1, 1, 3].
  void MakeBroadcastCompatible(size_t minimum_rank,
                               base::span<const uint32_t> axes);

  void SetTotalTensorSizeInBytes(uint64_t new_total_tensor_size_bytes);

 private:
  FRIEND_TEST_ALL_PREFIXES(WebNNTensorDescTest, CreateAndCopyTensorDescA);
  FRIEND_TEST_ALL_PREFIXES(WebNNTensorDescTest, CreateAndCopyTensorDescB);
  FRIEND_TEST_ALL_PREFIXES(WebNNTensorDescTest, CreateAndCopyTensorDescC);

  // DML_BUFFER_TENSOR_DESC consists of the pointers to a DirectML tensor's
  // dimensions and strides.
  std::vector<uint32_t> dimensions_;
  std::vector<uint32_t> strides_;

  // The DML_BUFFER_TENSOR_DESC describes a tensor that will be stored in a
  // Direct3D 12 buffer resource.
  DML_BUFFER_TENSOR_DESC buffer_desc_ = {};

  // The DML_TENSOR_DESC mainly consists of a pointer to the
  // DML_BUFFER_TENSOR_DESC.
  DML_TENSOR_DESC tensor_desc_ = {};
};

}  // namespace webnn::dml

#endif  // SERVICES_WEBNN_DML_TENSOR_DESC_H_
