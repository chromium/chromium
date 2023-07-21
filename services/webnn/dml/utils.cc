// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/dml/utils.h"

#include "base/check.h"
#include "base/check_op.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "services/webnn/dml/error.h"

namespace webnn::dml {

namespace {

uint64_t CalculateElementCount(
    const std::vector<uint32_t>& dimensions,
    const absl::optional<std::vector<uint32_t>>& strides) {
  base::CheckedNumeric<uint64_t> checked_element_count = 1;
  if (!strides) {
    for (auto& d : dimensions) {
      checked_element_count *= d;
    }
  } else {
    CHECK_EQ(dimensions.size(), strides.value().size());
    base::CheckedNumeric<uint32_t> indexOfLastElement = 0;
    for (size_t i = 0; i < dimensions.size(); ++i) {
      indexOfLastElement += (dimensions[i] - 1) * strides.value()[i];
    }
    checked_element_count = indexOfLastElement + 1;
  }

  return checked_element_count.ValueOrDie();
}

}  // namespace

uint64_t CalculateDMLBufferTensorSize(
    DML_TENSOR_DATA_TYPE data_type,
    const std::vector<uint32_t>& dimensions,
    const absl::optional<std::vector<uint32_t>>& strides) {
  size_t element_size;
  switch (data_type) {
    case DML_TENSOR_DATA_TYPE_FLOAT32:
    case DML_TENSOR_DATA_TYPE_UINT32:
    case DML_TENSOR_DATA_TYPE_INT32:
      element_size = 4;
      break;
    case DML_TENSOR_DATA_TYPE_FLOAT16:
    case DML_TENSOR_DATA_TYPE_UINT16:
    case DML_TENSOR_DATA_TYPE_INT16:
      element_size = 2;
      break;
    case DML_TENSOR_DATA_TYPE_UINT8:
    case DML_TENSOR_DATA_TYPE_INT8:
      element_size = 1;
      break;
    case DML_TENSOR_DATA_TYPE_FLOAT64:
    case DML_TENSOR_DATA_TYPE_UINT64:
    case DML_TENSOR_DATA_TYPE_INT64:
      element_size = 8;
      break;
    default:
      NOTREACHED_NORETURN();
  }

  // Calculate the total size of the tensor in bytes. It should be rounded up to
  // the nearest 4 bytes according to the alignment requirement:
  // https://learn.microsoft.com/en-us/windows/ai/directml/dml-helper-functions#dmlcalcbuffertensorsize
  base::CheckedNumeric<uint64_t> buffer_tensor_size =
      (CalculateElementCount(dimensions, strides) * element_size + 3) & ~3ull;

  return buffer_tensor_size.ValueOrDie();
}

ComPtr<ID3D12Device> GetD3D12Device(IDMLDevice* dml_device) {
  CHECK(dml_device);
  ComPtr<ID3D12Device> d3d12_device;
  CHECK_EQ(dml_device->GetParentDevice(IID_PPV_ARGS(&d3d12_device)), S_OK);
  return d3d12_device;
}

}  // namespace webnn::dml
