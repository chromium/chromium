// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_error.h"

namespace blink {

// static
GPUError* GPUError::Create(const String& message) {
  return MakeGarbageCollected<GPUError>(message);
}

GPUError::GPUError(const String& message) : message_(message) {}

const String& GPUError::message() const {
  return message_;
}

}  // namespace blink
