// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/dml/utils.h"

#include <string.h>
#include <set>

#include "base/bits.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "services/webnn/dml/error.h"

namespace webnn::dml {

namespace {

const char kBackendName[] = "DirectML: ";

// Note that the element count is considered as 1 when the give dimensions is
// empty.
uint64_t CalculateElementCount(const std::vector<uint32_t>& dimensions,
                               const std::vector<uint32_t>& strides = {}) {
  base::CheckedNumeric<uint64_t> checked_element_count = 1;
  if (strides.empty()) {
    for (const auto& d : dimensions) {
      checked_element_count *= d;
    }
  } else {
    CHECK_EQ(dimensions.size(), strides.size());
    base::CheckedNumeric<uint32_t> index_of_last_element = 0;
    for (size_t i = 0; i < dimensions.size(); ++i) {
      index_of_last_element += (dimensions[i] - 1) * strides[i];
    }
    checked_element_count = index_of_last_element + 1;
  }

  return checked_element_count.ValueOrDie();
}

// Check 1. no duplicate value in `axes`​, 2. values in `axes` ​​are all
// within [0, N - 1], where N is the length of `axes`.
bool ValidateAxes(base::span<const uint32_t> axes) {
  size_t rank = axes.size();

  if (base::ranges::any_of(axes, [rank](uint32_t axis) {
        return base::checked_cast<size_t>(axis) >= rank;
      })) {
    // All axes should be within range [0, N - 1].
    return false;
  }

  // TODO(crbug.com/1273291): Replace `std::set` with `std::bitset` for
  // duplication check after the maximum number of operand dimensions has been
  // settled and validated before using this function. Use `std::set` here at
  // present to avoid dimensions count check. Dimensions number issue tracked in
  // https://github.com/webmachinelearning/webnn/issues/456.
  if (rank != std::set<uint32_t>(axes.begin(), axes.end()).size()) {
    // Axes should not contain duplicate values.
    return false;
  }

  return true;
}

}  // namespace

uint64_t CalculateDMLBufferTensorSize(
    DML_TENSOR_DATA_TYPE data_type,
    const std::vector<uint32_t>& dimensions,
    const std::vector<uint32_t>& strides = {}) {
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
      base::bits::AlignUp<uint64_t>(
          CalculateElementCount(dimensions, strides) * element_size, 4);

  return buffer_tensor_size.ValueOrDie();
}

std::vector<uint32_t> CalculateStrides(base::span<const uint32_t> dimensions) {
  size_t dim_size = dimensions.size();
  std::vector<uint32_t> strides(dim_size);
  base::CheckedNumeric<uint32_t> stride = 1;
  for (size_t i = dim_size; i-- > 0;) {
    strides[i] = stride.ValueOrDie();
    stride *= dimensions[i];
  }
  return strides;
}

std::vector<uint32_t> PermuteArray(base::span<const uint32_t> array,
                                   base::span<const uint32_t> permutation) {
  CHECK_EQ(array.size(), permutation.size());
  CHECK(ValidateAxes(permutation));

  size_t arr_size = array.size();
  std::vector<uint32_t> permuted_array(arr_size);
  for (size_t i = 0; i < arr_size; ++i) {
    permuted_array[i] = array[permutation[i]];
  }

  return permuted_array;
}

ComPtr<ID3D12Device> GetD3D12Device(IDMLDevice* dml_device) {
  CHECK(dml_device);
  ComPtr<ID3D12Device> d3d12_device;
  CHECK_EQ(dml_device->GetParentDevice(IID_PPV_ARGS(&d3d12_device)), S_OK);
  return d3d12_device;
}

DML_FEATURE_LEVEL GetMaxSupportedDMLFeatureLevel(IDMLDevice* dml_device) {
  CHECK(dml_device);

  // WebNN targets DirectML version 1.6 or DML_FEATURE_LEVEL_4_0.
  // So query all levels up to DML_FEATURE_LEVEL_4_0. This allows
  // downlevel hardware to still run unit-tests that may only require a lower
  // level.
  DML_FEATURE_LEVEL feature_levels_requested[] = {
      DML_FEATURE_LEVEL_1_0, DML_FEATURE_LEVEL_2_0, DML_FEATURE_LEVEL_2_1,
      DML_FEATURE_LEVEL_3_0, DML_FEATURE_LEVEL_3_1, DML_FEATURE_LEVEL_4_0};

  DML_FEATURE_QUERY_FEATURE_LEVELS feature_levels_query = {
      std::size(feature_levels_requested), feature_levels_requested};

  // DML_FEATURE_FEATURE_LEVELS was introduced in DirectML version 1.1
  // and is not supported by DirectML version 1.0 which uses
  // DML_FEATURE_LEVEL_1_0.
  // https://learn.microsoft.com/en-us/windows/ai/directml/dml-feature-level-history
  DML_FEATURE_DATA_FEATURE_LEVELS feature_levels_supported = {};
  if (FAILED(dml_device->CheckFeatureSupport(
          DML_FEATURE_FEATURE_LEVELS, sizeof(feature_levels_query),
          &feature_levels_query, sizeof(feature_levels_supported),
          &feature_levels_supported))) {
    return DML_FEATURE_LEVEL_1_0;
  }

  return feature_levels_supported.MaxSupportedFeatureLevel;
}

D3D12_RESOURCE_BARRIER CreateTransitionBarrier(ID3D12Resource* resource,
                                               D3D12_RESOURCE_STATES before,
                                               D3D12_RESOURCE_STATES after) {
  return {.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
          .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
          .Transition = {.pResource = resource,
                         .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                         .StateBefore = before,
                         .StateAfter = after}};
}

void UploadBufferWithBarrier(CommandRecorder* command_recorder,
                             ComPtr<ID3D12Resource> dst_buffer,
                             ComPtr<ID3D12Resource> src_buffer,
                             size_t buffer_size) {
  // Copy the data from source buffer to destination buffer.
  D3D12_RESOURCE_BARRIER barriers[1];
  barriers[0] = CreateTransitionBarrier(dst_buffer.Get(),
                                        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                                        D3D12_RESOURCE_STATE_COPY_DEST);
  command_recorder->ResourceBarrier(barriers);
  command_recorder->CopyBufferRegion(dst_buffer, 0, std::move(src_buffer), 0,
                                     buffer_size);
  // The bound resources should be in D3D12_RESOURCE_STATE_UNORDERED_ACCESS
  // state before the execution of RecordDispatch on the GPU.
  barriers[0] =
      CreateTransitionBarrier(dst_buffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
                              D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
  command_recorder->ResourceBarrier(barriers);
}

mojom::ErrorPtr CreateError(mojom::Error::Code error_code,
                            std::string error_messages) {
  return mojom::Error::New(error_code,
                           kBackendName + std::move(error_messages));
}

}  // namespace webnn::dml
