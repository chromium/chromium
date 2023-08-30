// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_DML_TENSOR_DESC_H_
#define SERVICES_WEBNN_DML_TENSOR_DESC_H_

#include <DirectML.h>
#include <wrl.h>
#include <vector>

#include "base/containers/span.h"
#include "base/gtest_prod_util.h"

namespace webnn::dml {

using Microsoft::WRL::ComPtr;

// The TensorDesc wraps a tensor description (DML_TENSOR_DESC) needed by a DML
// graph. It owns the tensor's dimensions, strides and DML_BUFFER_TENSOR_DESC.
// The TensorDesc is prepared for building a DML graph's description
// (DML_GRAPH_DESC).
class TensorDesc final {
 public:
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
