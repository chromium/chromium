// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/dawn_control_client_holder.h"

#include "base/check.h"
#include "gpu/command_buffer/client/webgpu_interface.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

DawnControlClientHolder::DawnControlClientHolder(
    std::unique_ptr<WebGraphicsContext3DProvider> context_provider)
    : context_provider_(std::make_unique<WebGraphicsContext3DProviderWrapper>(
          std::move(context_provider))),
      interface_(GetContextProvider()->WebGPUInterface()),
      procs_(interface_->GetProcs()) {}

void DawnControlClientHolder::SetLostContextCallback() {
  GetContextProvider()->SetLostContextCallback(WTF::BindRepeating(
      &DawnControlClientHolder::SetContextLost, base::WrapRefCounted(this)));
}

void DawnControlClientHolder::Destroy() {
  SetContextLost();
  interface_->DisconnectContextAndDestroyServer();
}

base::WeakPtr<WebGraphicsContext3DProviderWrapper>
DawnControlClientHolder::GetContextProviderWeakPtr() const {
  return context_provider_->GetWeakPtr();
}

WebGraphicsContext3DProvider* DawnControlClientHolder::GetContextProvider()
    const {
  return context_provider_->ContextProvider();
}

gpu::webgpu::WebGPUInterface* DawnControlClientHolder::GetInterface() const {
  DCHECK(interface_);
  return interface_;
}

void DawnControlClientHolder::SetContextLost() {
  lost_ = true;
}

bool DawnControlClientHolder::IsContextLost() const {
  return lost_;
}

}  // namespace blink
