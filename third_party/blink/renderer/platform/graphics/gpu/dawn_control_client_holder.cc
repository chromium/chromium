// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/dawn_control_client_holder.h"

#include "base/check.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_resource_provider_cache.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

// static
scoped_refptr<DawnControlClientHolder> DawnControlClientHolder::Create(
    std::unique_ptr<WebGraphicsContext3DProvider> context_provider,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  auto dawn_control_client_holder =
      base::MakeRefCounted<DawnControlClientHolder>(std::move(context_provider),
                                                    std::move(task_runner));
  // The context lost callback occurs when the client receives
  // OnGpuControlLostContext. This can happen on fatal errors when the GPU
  // channel is disconnected: the GPU process crashes, the GPU process fails to
  // deserialize a message, etc. We mark the context lost, but NOT destroy the
  // entire WebGraphicsContext3DProvider as that would free services for mapping
  // shared memory. There may still be outstanding mapped GPUBuffers pointing to
  // this memory.
  dawn_control_client_holder->context_provider_->ContextProvider()
      ->SetLostContextCallback(WTF::BindRepeating(
          &DawnControlClientHolder::MarkContextLost,
          dawn_control_client_holder->weak_ptr_factory_.GetWeakPtr()));
  return dawn_control_client_holder;
}

DawnControlClientHolder::DawnControlClientHolder(
    std::unique_ptr<WebGraphicsContext3DProvider> context_provider,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : context_provider_(std::make_unique<WebGraphicsContext3DProviderWrapper>(
          std::move(context_provider))),
      task_runner_(task_runner),
      api_channel_(context_provider_->ContextProvider()
                       ->WebGPUInterface()
                       ->GetAPIChannel()),
      procs_(api_channel_->GetProcs()),
      recyclable_resource_cache_(GetContextProviderWeakPtr(), task_runner) {}

DawnControlClientHolder::~DawnControlClientHolder() = default;

void DawnControlClientHolder::Destroy() {
  MarkContextLost();

  // Destroy the WebGPU context.
  // This ensures that GPU resources are eagerly reclaimed.
  // Because we have disconnected the wire client, any JavaScript which uses
  // WebGPU will do nothing.
  if (context_provider_) {
    // If the context provider is destroyed during a real lost context event, it
    // causes the CommandBufferProxy that the context provider owns, which is
    // what issued the lost context event in the first place, to be destroyed
    // before the event is done being handled. This causes a crash when an
    // outstanding AutoLock goes out of scope. To avoid this, we create a no-op
    // task to hold a reference to the context provider until this function is
    // done executing, and drop it after.
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce([](std::unique_ptr<WebGraphicsContext3DProviderWrapper>
                              context_provider) {},
                       std::move(context_provider_)));
  }
}

base::WeakPtr<WebGraphicsContext3DProviderWrapper>
DawnControlClientHolder::GetContextProviderWeakPtr() const {
  if (!context_provider_) {
    return nullptr;
  }
  return context_provider_->GetWeakPtr();
}

WGPUInstance DawnControlClientHolder::GetWGPUInstance() const {
  return api_channel_->GetWGPUInstance();
}

void DawnControlClientHolder::MarkContextLost() {
  if (context_lost_) {
    return;
  }
  api_channel_->Disconnect();
  context_lost_ = true;
}

bool DawnControlClientHolder::IsContextLost() const {
  return context_lost_;
}

std::unique_ptr<RecyclableCanvasResource>
DawnControlClientHolder::GetOrCreateCanvasResource(const SkImageInfo& info) {
  return recyclable_resource_cache_.GetOrCreateCanvasResource(info);
}

void DawnControlClientHolder::Flush() {
  auto context_provider = GetContextProviderWeakPtr();
  if (LIKELY(context_provider)) {
    context_provider->ContextProvider()->WebGPUInterface()->FlushCommands();
  }
}

void DawnControlClientHolder::EnsureFlush(scheduler::EventLoop& event_loop) {
  auto context_provider = GetContextProviderWeakPtr();
  if (UNLIKELY(!context_provider))
    return;
  if (!context_provider->ContextProvider()
           ->WebGPUInterface()
           ->EnsureAwaitingFlush()) {
    // We've already enqueued a task to flush, or the command buffer
    // is empty. Do nothing.
    return;
  }
  event_loop.EnqueueMicrotask(WTF::BindOnce(
      [](scoped_refptr<DawnControlClientHolder> dawn_control_client) {
        if (auto context_provider =
                dawn_control_client->GetContextProviderWeakPtr()) {
          context_provider->ContextProvider()
              ->WebGPUInterface()
              ->FlushAwaitingCommands();
        }
      },
      scoped_refptr<DawnControlClientHolder>(this)));
}

}  // namespace blink
