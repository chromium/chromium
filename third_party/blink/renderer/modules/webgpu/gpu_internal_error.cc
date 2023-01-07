// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_internal_error.h"

namespace blink {

// static
GPUInternalError* GPUInternalError::Create(const String& message) {
  return MakeGarbageCollected<GPUInternalError>(message);
}

GPUInternalError::GPUInternalError(const String& message) : GPUError(message) {}

}  // namespace blink
