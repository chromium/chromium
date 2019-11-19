// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_VALIDATION_ERROR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_VALIDATION_ERROR_H_

#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class GPUValidationError : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static GPUValidationError* Create(const AtomicString& message);
  GPUValidationError(const AtomicString& message);

  // gpu_validation_error.idl
  const String& message() const;

 private:
  String message_;

  DISALLOW_COPY_AND_ASSIGN(GPUValidationError);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_VALIDATION_ERROR_H_
