// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_OUT_OF_MEMORY_ERROR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_OUT_OF_MEMORY_ERROR_H_

#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class GPUOutOfMemoryError : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static GPUOutOfMemoryError* Create();
  GPUOutOfMemoryError();

 private:
  DISALLOW_COPY_AND_ASSIGN(GPUOutOfMemoryError);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_OUT_OF_MEMORY_ERROR_H_
