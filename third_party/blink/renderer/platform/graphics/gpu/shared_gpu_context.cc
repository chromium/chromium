// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"

#include "base/single_thread_task_runner.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "gpu/config/gpu_feature_info.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

SharedGpuContext* SharedGpuContext::GetInstanceForCurrentThread() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<SharedGpuContext>,
                                  thread_specific_instance, ());
  return thread_specific_instance;
}

SharedGpuContext::SharedGpuContext() = default;

// static
bool SharedGpuContext::IsGpuCompositingEnabled() {
  SharedGpuContext* this_ptr = GetInstanceForCurrentThread();
  // The check for gpu compositing enabled implies a context will
  // desired, so we combine them into a single trip to the main thread.
  // This also ensures that the compositing mode does not change before
  // the context is created, so if it does change the context will be lost
  // and this class will know to check the compositing mode again.
  bool only_if_gpu_compositing = true;
  this_ptr->CreateContextProviderIfNeeded(only_if_gpu_compositing);
  return !this_ptr->is_gpu_compositing_disabled_;
}

base::WeakPtr<WebGraphicsContext3DProviderWrapper>
SharedGpuContext::ContextProviderWrapper() {
  SharedGpuContext* this_ptr = GetInstanceForCurrentThread();
  bool only_if_gpu_compositing = false;
  this_ptr->CreateContextProviderIfNeeded(only_if_gpu_compositing);
  if (!this_ptr->context_provider_wrapper_)
    return nullptr;
  return this_ptr->context_provider_wrapper_->GetWeakPtr();
}

static void CreateContextProviderOnMainThread(
    bool only_if_gpu_compositing,
    bool* gpu_compositing_disabled,
    std::unique_ptr<WebGraphicsContext3DProviderWrapper>* wrapper,
    base::WaitableEvent* waitable_event) {
  DCHECK(IsMainThread());

  Platform::ContextAttributes context_attributes;
  context_attributes.enable_raster_interface = true;
  context_attributes.support_grcontext = true;

  // The shared GPU context should not trigger a switch to the high-performance
  // GPU.
  context_attributes.prefer_low_power_gpu = true;

  *gpu_compositing_disabled = Platform::Current()->IsGpuCompositingDisabled();
  if (*gpu_compositing_disabled && only_if_gpu_compositing) {
    waitable_event->Signal();
    return;
  }

  Platform::GraphicsInfo graphics_info;
  auto context_provider =
      Platform::Current()->CreateOffscreenGraphicsContext3DProvider(
          context_attributes, WebURL(), &graphics_info);
  if (context_provider) {
    *wrapper = std::make_unique<WebGraphicsContext3DProviderWrapper>(
        std::move(context_provider));
  }
  waitable_event->Signal();
}

void SharedGpuContext::CreateContextProviderIfNeeded(
    bool only_if_gpu_compositing) {
  // Once true, |is_gpu_compositing_disabled_| will always stay true.
  if (is_gpu_compositing_disabled_ && only_if_gpu_compositing)
    return;

  // TODO(danakj): This needs to check that the context is being used on the
  // thread it was made on, or else lock it.
  if (context_provider_wrapper_ &&
      context_provider_wrapper_->ContextProvider()
              ->RasterInterface()
              ->GetGraphicsResetStatusKHR() == GL_NO_ERROR) {
    // If the context isn't lost then |is_gpu_compositing_disabled_| state
    // hasn't changed yet. RenderThreadImpl::CompositingModeFallbackToSoftware()
    // will lose the context to let us know if it changes.
    return;
  }

  is_gpu_compositing_disabled_ = false;
  context_provider_wrapper_ = nullptr;

  if (context_provider_factory_) {
    // This path should only be used in unit tests.
    auto context_provider =
        context_provider_factory_.Run(&is_gpu_compositing_disabled_);
    if (context_provider) {
      context_provider_wrapper_ =
          std::make_unique<WebGraphicsContext3DProviderWrapper>(
              std::move(context_provider));
    }
  } else if (IsMainThread()) {
    is_gpu_compositing_disabled_ =
        Platform::Current()->IsGpuCompositingDisabled();
    if (is_gpu_compositing_disabled_ && only_if_gpu_compositing)
      return;
    auto context_provider =
        Platform::Current()->CreateSharedOffscreenGraphicsContext3DProvider();
    if (context_provider) {
      context_provider_wrapper_ =
          std::make_unique<WebGraphicsContext3DProviderWrapper>(
              std::move(context_provider));
    }
  } else {
    // This synchronous round-trip to the main thread is the reason why
    // SharedGpuContext encasulates the context provider: so we only have to do
    // this once per thread.
    base::WaitableEvent waitable_event;
    scoped_refptr<base::SingleThreadTaskRunner> task_runner =
        Thread::MainThread()->GetTaskRunner();
    PostCrossThreadTask(
        *task_runner, FROM_HERE,
        CrossThreadBindOnce(
            &CreateContextProviderOnMainThread, only_if_gpu_compositing,
            CrossThreadUnretained(&is_gpu_compositing_disabled_),
            CrossThreadUnretained(&context_provider_wrapper_),
            CrossThreadUnretained(&waitable_event)));
    waitable_event.Wait();
    if (context_provider_wrapper_ &&
        !context_provider_wrapper_->ContextProvider()->BindToCurrentThread())
      context_provider_wrapper_ = nullptr;
  }
}

// static
void SharedGpuContext::SetContextProviderFactoryForTesting(
    ContextProviderFactory factory) {
  SharedGpuContext* this_ptr = GetInstanceForCurrentThread();
  DCHECK(!this_ptr->context_provider_wrapper_);
  this_ptr->context_provider_factory_ = std::move(factory);
}

// static
void SharedGpuContext::ResetForTesting() {
  SharedGpuContext* this_ptr = GetInstanceForCurrentThread();
  this_ptr->is_gpu_compositing_disabled_ = false;
  this_ptr->context_provider_wrapper_.reset();
  this_ptr->context_provider_factory_.Reset();
}

bool SharedGpuContext::IsValidWithoutRestoring() {
  SharedGpuContext* this_ptr = GetInstanceForCurrentThread();
  if (!this_ptr->context_provider_wrapper_)
    return false;
  return this_ptr->context_provider_wrapper_->ContextProvider()
             ->ContextGL()
             ->GetGraphicsResetStatusKHR() == GL_NO_ERROR;
}

bool SharedGpuContext::AllowSoftwareToAcceleratedCanvasUpgrade() {
  SharedGpuContext* this_ptr = GetInstanceForCurrentThread();
  bool only_if_gpu_compositing = false;
  this_ptr->CreateContextProviderIfNeeded(only_if_gpu_compositing);
  if (!this_ptr->context_provider_wrapper_)
    return false;
  return !this_ptr->context_provider_wrapper_->ContextProvider()
              ->GetGpuFeatureInfo()
              .IsWorkaroundEnabled(
                  gpu::DISABLE_SOFTWARE_TO_ACCELERATED_CANVAS_UPGRADE);
}

}  // blink
