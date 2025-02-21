// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_ORT_BUFFER_CONTENT_ORT_H_
#define SERVICES_WEBNN_ORT_BUFFER_CONTENT_ORT_H_

#include "services/webnn/ort/scoped_ort_types.h"

namespace webnn::ort {

// The internal contents of an MLTensor. Access should be managed by wrapping in
// a `QueueableResourceState`.
class BufferContentOrt {
 public:
  explicit BufferContentOrt(ScopedOrtValuePtr tensor);

  BufferContentOrt(const BufferContentOrt&) = delete;
  BufferContentOrt& operator=(const BufferContentOrt&) = delete;

  ~BufferContentOrt();

  OrtValue* tensor() const { return tensor_.Get(); }

 private:
  ScopedOrtValuePtr tensor_;
};

}  // namespace webnn::ort

#endif  // SERVICES_WEBNN_ORT_BUFFER_CONTENT_ORT_H_
