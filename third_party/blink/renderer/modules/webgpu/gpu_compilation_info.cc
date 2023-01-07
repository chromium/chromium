// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_compilation_info.h"

#include "third_party/blink/renderer/modules/webgpu/gpu_compilation_message.h"

namespace blink {

void GPUCompilationInfo::AppendMessage(GPUCompilationMessage* message) {
  messages_.push_back(message);
}

void GPUCompilationInfo::Trace(Visitor* visitor) const {
  visitor->Trace(messages_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
