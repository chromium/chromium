// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_TFLITE_BUFFER_CONTENT_H_
#define SERVICES_WEBNN_TFLITE_BUFFER_CONTENT_H_

#include "base/containers/span.h"
#include "base/memory/aligned_memory.h"

namespace webnn::tflite {

// The internal contents of an MLTensor. Access should be managed by wrapping in
// a `QueueableResourceState`.
class BufferContent {
 public:
  explicit BufferContent(size_t size);

  BufferContent(const BufferContent&) = delete;
  BufferContent& operator=(const BufferContent&) = delete;

  ~BufferContent();

  base::span<uint8_t> AsSpan() const;

 private:
  // TODO(https://crbug.com/40278771): Use a real hardware buffer on platforms
  // where that would be beneficial.
  const std::unique_ptr<void, base::AlignedFreeDeleter> buffer_;
  const size_t size_;
};

}  // namespace webnn::tflite

#endif  // SERVICES_WEBNN_TFLITE_BUFFER_CONTENT_H_
