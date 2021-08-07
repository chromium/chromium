// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_texture_usage.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"

namespace blink {

namespace {
void AddConsoleWarning(ExecutionContext* execution_context,
                       const char* message) {
  if (execution_context) {
    auto* console_message = MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kRendering,
        mojom::blink::ConsoleMessageLevel::kWarning, message);
    execution_context->AddConsoleMessage(console_message);
  }
}
}  // namespace

// static
unsigned GPUTextureUsage::SAMPLED(ExecutionContext* execution_context) {
  AddConsoleWarning(execution_context,
                    "GPUTextureUsage.SAMPLED is deprecated. "
                    "Use GPUTextureUsage.TEXTURE_BINDING instead.");
  return kTextureBinding;
}

// static
unsigned GPUTextureUsage::STORAGE(ExecutionContext* execution_context) {
  AddConsoleWarning(execution_context,
                    "GPUTextureUsage.STORAGE is deprecated. "
                    "Use GPUTextureUsage.STORAGE_BINDING instead.");
  return kStorageBinding;
}

}  // namespace blink
