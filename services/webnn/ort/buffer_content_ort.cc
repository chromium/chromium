// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/buffer_content_ort.h"

#include <ranges>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/numerics/safe_conversions.h"
#include "services/webnn/ort/ort_data_type.h"
#include "services/webnn/ort/ort_status.h"
#include "services/webnn/ort/ort_tensor.h"
#include "services/webnn/ort/platform_functions_ort.h"
#include "third_party/onnxruntime_headers/src/include/onnxruntime/core/session/onnxruntime_c_api.h"

namespace webnn::ort {

BufferContentOrt::BufferContentOrt(const OperandDescriptor& descriptor) {
  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();
  OrtAllocator* allocator = nullptr;
  // Use the default allocator which is CPU based and non-arena.
  // `GetAllocatorWithDefaultOptions()` always returns the same pointer to the
  // same default allocator and its returned value should NOT be freed.
  //
  // TODO(crbug.com/419403184): Figure out how to support allocator for other
  // devices.
  CHECK_STATUS(ort_api->GetAllocatorWithDefaultOptions(&allocator));
  CHECK(allocator);

  ONNXTensorElementDataType ort_data_type =
      WebnnToOnnxDataType(descriptor.data_type());
  std::vector<int64_t> ort_shape = WebnnToOnnxShape(descriptor.shape());

  CHECK_STATUS(ort_api->CreateTensorAsOrtValue(
      allocator, ort_shape.data(), ort_shape.size(), ort_data_type,
      ScopedOrtValue::Receiver(tensor_).get()));
  CHECK(tensor_.get());

  // TODO(crbug.com/420355411): Use ORT GetTensorSizeInBytes API once it is
  // supported.
  size_ = CalculateOrtTensorSizeInBytes(ort_shape, ort_data_type);
  // Invalid values are rejected in GraphBuilder.
  CHECK(base::IsValueInRangeForNumericType<int>(size_));

  // Initialize the tensor with zeros, otherwise, reading uninitialized memory
  // will get random values.
  std::ranges::fill(AsSpan(), 0);
}

BufferContentOrt::~BufferContentOrt() = default;

base::span<uint8_t> BufferContentOrt::AsSpan() const {
  void* ort_tensor_raw_data = nullptr;
  CHECK_STATUS(
      PlatformFunctions::GetInstance()->ort_api()->GetTensorMutableData(
          tensor_.get(), &ort_tensor_raw_data));
  CHECK(ort_tensor_raw_data);
  // SAFETY: ORT guarantees that it has allocated enough memory to
  // store tensor.
  return UNSAFE_BUFFERS(
      base::span(static_cast<uint8_t*>(ort_tensor_raw_data), size_));
}

}  // namespace webnn::ort
