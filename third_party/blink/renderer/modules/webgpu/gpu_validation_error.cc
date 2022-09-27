// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_validation_error.h"

namespace blink {

// static
GPUValidationError* GPUValidationError::Create(const String& message) {
  return MakeGarbageCollected<GPUValidationError>(message);
}

GPUValidationError::GPUValidationError(const String& message)
    : GPUError(message) {}

}  // namespace blink
