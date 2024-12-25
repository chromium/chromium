// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_ORT_BUFFER_CONTENT_ORT_H_
#define SERVICES_WEBNN_ORT_BUFFER_CONTENT_ORT_H_

#include "services/webnn/ort/utils_ort.h"

namespace webnn::ort {

class AllocatorOrt;

// The internal contents of an MLTensor. Access should be managed by wrapping in
// a `QueueableResourceState`.
class BufferContentOrt {
 public:
  explicit BufferContentOrt(OrtAllocator* allocator,
                            std::vector<int64_t> shape,
                            ONNXTensorElementDataType ort_data_type);

  BufferContentOrt(const BufferContentOrt&) = delete;
  BufferContentOrt& operator=(const BufferContentOrt&) = delete;

  ~BufferContentOrt();

  OrtValue* tensor() const { return tensor_; }

 private:
  raw_ptr<OrtValue> tensor_;
  std::vector<int64_t> shape_;
};

}  // namespace webnn::ort

#endif  // SERVICES_WEBNN_ORT_BUFFER_CONTENT_ORT_H_
