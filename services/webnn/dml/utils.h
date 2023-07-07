// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_DML_UTILS_H_
#define SERVICES_WEBNN_DML_UTILS_H_

#include <DirectML.h>
#include <vector>

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace webnn::dml {

uint64_t CalculateDMLBufferTensorSize(
    DML_TENSOR_DATA_TYPE data_type,
    const std::vector<uint32_t>& dimensions,
    const absl::optional<std::vector<uint32_t>>& strides);

}  // namespace webnn::dml

#endif  // SERVICES_WEBNN_DML_UTILS_H_
