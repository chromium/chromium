// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_COREML_BUFFER_CONTENT_H_
#define SERVICES_WEBNN_COREML_BUFFER_CONTENT_H_

#include <CoreML/CoreML.h>

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "mojo/public/cpp/base/big_buffer.h"

namespace webnn::coreml {

// The internal contents of a CoreML MLTensor. Access should be managed by
// wrapping in a `QueueableResourceState`.
class API_AVAILABLE(macos(12.3)) BufferContent {
 public:
  explicit BufferContent(MLMultiArray* multi_array);

  BufferContent(const BufferContent&) = delete;
  BufferContent& operator=(const BufferContent&) = delete;

  ~BufferContent();

  void Read(
      base::OnceCallback<void(mojo_base::BigBuffer)> result_callback) const;

  void Write(base::span<const uint8_t> bytes_to_write,
             base::OnceClosure done_closure);

  MLFeatureValue* AsFeatureValue() const;

 private:
  MLMultiArray* multi_array_;
};

}  // namespace webnn::coreml

#endif  // SERVICES_WEBNN_COREML_BUFFER_CONTENT_H_
