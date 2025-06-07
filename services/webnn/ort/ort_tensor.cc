// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/ort_tensor.h"

#include <numeric>
#include <ostream>

#include "base/notreached.h"
#include "base/numerics/checked_math.h"

namespace webnn::ort {

size_t CalculateOrtTensorSizeInBytes(base::span<const int64_t> shape,
                                     ONNXTensorElementDataType data_type) {
  base::CheckedNumeric<uint64_t> element_size_in_bits;
  switch (data_type) {
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64:
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT64: {
      element_size_in_bits = 64;
      break;
    }
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT:
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32:
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT32: {
      element_size_in_bits = 32;
      break;
    }
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16: {
      element_size_in_bits = 16;
      break;
    }
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT8:
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT8: {
      element_size_in_bits = 8;
      break;
    }
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT4:
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT4: {
      element_size_in_bits = 4;
      break;
    }
    default: {
      NOTREACHED()
          << "CalculateOrtTensorSizeInBytes() only supports WebNN data types.";
    }
  }
  auto tensor_size_in_bits = std::accumulate(
      shape.begin(), shape.end(), element_size_in_bits, std::multiplies());

  return ((tensor_size_in_bits + 7) / 8).ValueOrDie<size_t>();
}

}  // namespace webnn::ort
