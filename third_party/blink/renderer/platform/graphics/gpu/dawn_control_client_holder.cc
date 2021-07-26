// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/dawn_control_client_holder.h"

#include "base/check.h"
#include "gpu/command_buffer/client/webgpu_interface.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

// static
scoped_refptr<DawnControlClientHolder> DawnControlClientHolder::Create(
    std::unique_ptr<WebGraphicsContext3DProvider> context_provider,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  auto dawn_control_client_holder =
      base::MakeRefCounted<DawnControlClientHolder>(std::move(context_provider),
                                                    std::move(task_runner));
  dawn_control_client_holder->context_provider_->ContextProvider()
      ->SetLostContextCallback(
          WTF::BindRepeating(&DawnControlClientHolder::SetContextLost,
                             dawn_control_client_holder));
  return dawn_control_client_holder;
}

DawnControlClientHolder::DawnControlClientHolder(
    std::unique_ptr<WebGraphicsContext3DProvider> context_provider,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : context_provider_(std::make_unique<WebGraphicsContext3DProviderWrapper>(
          std::move(context_provider))),
      procs_(
          context_provider_->ContextProvider()->WebGPUInterface()->GetProcs()),
      recyclable_resource_cache_(GetContextProviderWeakPtr(), task_runner) {}

void DawnControlClientHolder::Destroy() {
  SetContextLost();
  if (context_provider_) {
    context_provider_->ContextProvider()
        ->WebGPUInterface()
        ->DisconnectContextAndDestroyServer();
  }
}

base::WeakPtr<WebGraphicsContext3DProviderWrapper>
DawnControlClientHolder::GetContextProviderWeakPtr() const {
  if (!context_provider_) {
    return nullptr;
  }
  return context_provider_->GetWeakPtr();
}

void DawnControlClientHolder::SetContextLost() {
  lost_ = true;
}

bool DawnControlClientHolder::IsContextLost() const {
  return lost_;
}

std::unique_ptr<RecyclableCanvasResource>
DawnControlClientHolder::GetOrCreateCanvasResource(
    const IntSize& size,
    const CanvasResourceParams& params,
    bool is_origin_top_left) {
  return recyclable_resource_cache_.GetOrCreateCanvasResource(
      size, params, is_origin_top_left);
}

}  // namespace blink
