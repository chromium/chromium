// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_validation_error.h"

namespace blink {

// static
GPUValidationError* GPUValidationError::Create(const AtomicString& message) {
  return MakeGarbageCollected<GPUValidationError>(message);
}

GPUValidationError::GPUValidationError(const AtomicString& message) {
  message_ = message;
}

const String& GPUValidationError::message() const {
  return message_;
}

}  // namespace blink
