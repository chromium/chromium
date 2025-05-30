// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_ORT_ORT_TENSOR_H_
#define SERVICES_WEBNN_ORT_ORT_TENSOR_H_

#include "base/containers/span.h"
#include "third_party/onnxruntime_headers/src/include/onnxruntime/core/session/onnxruntime_c_api.h"

namespace webnn::ort {

size_t CalculateOrtTensorSizeInBytes(base::span<const int64_t> shape,
                                     ONNXTensorElementDataType data_type);

}  // namespace webnn::ort

#endif  // SERVICES_WEBNN_ORT_ORT_TENSOR_H_
