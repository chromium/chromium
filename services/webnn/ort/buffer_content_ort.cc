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
#include "services/webnn/ort/platform_functions_ort.h"
#include "third_party/windows_app_sdk_headers/src/inc/abi/winml/winml/onnxruntime_c_api.h"

namespace webnn::ort {

BufferContentOrt::BufferContentOrt(
    const OperandDescriptor& descriptor,
    scoped_refptr<DeviceAllocator> device_allocator)
    : device_allocator_(std::move(device_allocator)) {
  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();

  OrtAllocator* allocator = nullptr;
  // Use the device allocator if it's present. Otherwise, use the default
  // allocator which is CPU based and non-arena.
  if (device_allocator_) {
    allocator = device_allocator_->get();
  } else {
    // `GetAllocatorWithDefaultOptions()` always returns the same pointer to the
    // same default allocator and its returned value should NOT be freed.
    CHECK_STATUS(ort_api->GetAllocatorWithDefaultOptions(&allocator));
  }
  CHECK(allocator);

  ONNXTensorElementDataType ort_data_type =
      WebnnToOnnxDataType(descriptor.data_type());
  std::vector<int64_t> ort_shape = WebnnToOnnxShape(descriptor.shape());

  // TODO(crbug.com/453420646): Implement context lost handling for ORT tensor
  // creation failures.
  CHECK_STATUS(ort_api->CreateTensorAsOrtValue(
      allocator, ort_shape.data(), ort_shape.size(), ort_data_type,
      ScopedOrtValue::Receiver(tensor_).get()));
  CHECK(tensor_.get());

  CHECK_STATUS(ort_api->GetTensorSizeInBytes(tensor_.get(), &size_));
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
