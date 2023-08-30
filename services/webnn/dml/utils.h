// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_DML_UTILS_H_
#define SERVICES_WEBNN_DML_UTILS_H_

#include <DirectML.h>
#include <d3d12.h>
#include <wrl.h>
#include <vector>

#include "base/containers/span.h"

namespace webnn::dml {

using Microsoft::WRL::ComPtr;

uint64_t CalculateDMLBufferTensorSize(DML_TENSOR_DATA_TYPE data_type,
                                      const std::vector<uint32_t>& dimensions,
                                      const std::vector<uint32_t>& strides);

std::vector<uint32_t> CalculateStrides(base::span<const uint32_t> dimensions);

// The length of `permutation` must be the same as `array`. The values in
// `permutation` must be within the range [0, N-1] where N is the length of
// `array`. There must be no two or more same values in `permutation`.
//
// e.g., Given an array of [10, 11, 12, 13] and a permutation of [0, 2, 3, 1],
// the permuted array would be [10, 12, 13, 11].
std::vector<uint32_t> PermuteArray(base::span<const uint32_t> array,
                                   base::span<const uint32_t> permutation);

// Gets the ID3D12Device used to create the IDMLDevice.
ComPtr<ID3D12Device> GetD3D12Device(IDMLDevice* dml_device);

// Returns the maximum feature level supported by the DML device.
DML_FEATURE_LEVEL GetMaxSupportedDMLFeatureLevel(IDMLDevice* dml_device);

}  // namespace webnn::dml

#endif  // SERVICES_WEBNN_DML_UTILS_H_
