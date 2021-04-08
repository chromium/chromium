// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_COMPILATION_INFO_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_COMPILATION_INFO_H_

#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class GPUCompilationMessage;

class GPUCompilationInfo : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  GPUCompilationInfo() = default;

  void AppendMessage(GPUCompilationMessage* message);

  const HeapVector<Member<GPUCompilationMessage>>& messages() const {
    return messages_;
  }

  void Trace(Visitor*) const override;

 private:
  HeapVector<Member<GPUCompilationMessage>> messages_;

  DISALLOW_COPY_AND_ASSIGN(GPUCompilationInfo);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_COMPILATION_INFO_H_
