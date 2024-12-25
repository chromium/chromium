// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "services/webnn/ort/buffer_content_ort.h"

#include "services/webnn/ort/allocator_ort.h"
#include "services/webnn/ort/error_ort.h"

namespace webnn::ort {

BufferContentOrt::BufferContentOrt(OrtAllocator* allocator,
                                   std::vector<int64_t> shape,
                                   ONNXTensorElementDataType ort_data_type)
    : shape_(std::move(shape)) {
  const OrtApi* ort_api = GetOrtApi();
  OrtValue* tensor = nullptr;
  ORT_ABORT_ON_ERROR(ort_api->CreateTensorAsOrtValue(
      allocator, shape_.data(), shape_.size(), ort_data_type, &tensor));
  tensor_ = tensor;
  CHECK(tensor_);
}

BufferContentOrt::~BufferContentOrt() {
  const OrtApi* ort_api = GetOrtApi();
  ort_api->ReleaseValue(tensor_);
}

}  // namespace webnn::ort
