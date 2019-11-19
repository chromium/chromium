// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/dawn_control_client_holder.h"

#include "base/logging.h"
#include "gpu/command_buffer/client/webgpu_interface.h"

namespace blink {

DawnControlClientHolder::DawnControlClientHolder(
    std::unique_ptr<WebGraphicsContext3DProvider> context_provider)
    : context_provider_(std::move(context_provider)),
      interface_(context_provider_->WebGPUInterface()) {}

void DawnControlClientHolder::Destroy() {
  interface_ = nullptr;
  context_provider_.reset();
}

bool DawnControlClientHolder::IsDestroyed() const {
  return !interface_;
}

WebGraphicsContext3DProvider* DawnControlClientHolder::GetContextProvider()
    const {
  return context_provider_.get();
}

gpu::webgpu::WebGPUInterface* DawnControlClientHolder::GetInterface() const {
  DCHECK(interface_);
  return interface_;
}

const DawnProcTable& DawnControlClientHolder::GetProcs() const {
  DCHECK(interface_);
  return interface_->GetProcs();
}

}  // namespace blink
