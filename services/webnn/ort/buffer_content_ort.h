// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_ORT_BUFFER_CONTENT_ORT_H_
#define SERVICES_WEBNN_ORT_BUFFER_CONTENT_ORT_H_

#include "services/webnn/ort/scoped_ort_types.h"
#include "services/webnn/public/cpp/operand_descriptor.h"

namespace webnn::ort {

// The internal contents of an MLTensor. Access should be managed by wrapping in
// a `QueueableResourceState`.
class BufferContentOrt {
 public:
  explicit BufferContentOrt(const OperandDescriptor& descriptor);

  BufferContentOrt(const BufferContentOrt&) = delete;
  BufferContentOrt& operator=(const BufferContentOrt&) = delete;

  ~BufferContentOrt();

  OrtValue* tensor() const { return tensor_.get(); }
  base::span<uint8_t> AsSpan() const;

 private:
  ScopedOrtValue tensor_;
  size_t size_;
};

}  // namespace webnn::ort

#endif  // SERVICES_WEBNN_ORT_BUFFER_CONTENT_ORT_H_
