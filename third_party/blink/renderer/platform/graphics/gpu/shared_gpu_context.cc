// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"

#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/ipc/client/client_shared_image_interface.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_graphics_context_3d_provider.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgraphics_shared_image_interface_provider_impl.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
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
  if (IsMainThread()) {
    // On the main thread we have the opportunity to keep
    // is_gpu_compositing_disabled_ up to date continuously without locking
    // up the thread, so we do it. This allows user code to adapt immediately
    // when there is a fallback to software compositing.
    this_ptr->is_gpu_compositing_disabled_ =
        Platform::Current()->IsGpuCompositingDisabled();
  } else {
    // The check for gpu compositing enabled implies a context will be
    // desired, so we combine them into a single trip to the main thread.
    //
    // TODO(crbug.com/1486981): It is possible for the value of
    // this_ptr->is_gpu_compositing_disabled_ to become stale without notice
    // if the compositor falls back to software compositing after this
    // initialization. There are currently no known observable bugs caused by
    // this, but in theory, we'd need a mechanism for propagating changes in
    // GPU compositing availability to worker threads.
    this_ptr->CreateContextProviderIfNeeded(/*only_if_gpu_compositing=*/true);
  }
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

// static
WebGraphicsSharedImageInterfaceProvider*
SharedGpuContext::SharedImageInterfaceProvider() {
  SharedGpuContext* this_ptr = GetInstanceForCurrentThread();
  this_ptr->CreateSharedImageInterfaceProviderIfNeeded();
  if (!this_ptr->shared_image_interface_provider_) {
    return nullptr;
  }

  return this_ptr->shared_image_interface_provider_.get();
}

gpu::GpuMemoryBufferManager* SharedGpuContext::GetGpuMemoryBufferManager() {
  SharedGpuContext* this_ptr = GetInstanceForCurrentThread();
  if (!this_ptr->gpu_memory_buffer_manager_) {
    this_ptr->CreateContextProviderIfNeeded(/*only_if_gpu_compositing =*/true);
  }
  return this_ptr->gpu_memory_buffer_manager_;
}

void SharedGpuContext::SetGpuMemoryBufferManagerForTesting(
    gpu::GpuMemoryBufferManager* mgr) {
  SharedGpuContext* this_ptr = GetInstanceForCurrentThread();
  DCHECK(!this_ptr->gpu_memory_buffer_manager_ || !mgr);
  this_ptr->gpu_memory_buffer_manager_ = mgr;
}

static void CreateContextProviderOnMainThread(
    bool only_if_gpu_compositing,
    bool* gpu_compositing_disabled,
    std::unique_ptr<WebGraphicsContext3DProviderWrapper>* wrapper,
    gpu::GpuMemoryBufferManager** gpu_memory_buffer_manager,
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

  // A reference to the GpuMemoryBufferManager can only be obtained on the main
  // thread, but it is safe to use on other threads.
  *gpu_memory_buffer_manager = Platform::Current()->GetGpuMemoryBufferManager();

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
      !context_provider_wrapper_->ContextProvider()->IsContextLost()) {
    // If the context isn't lost then |is_gpu_compositing_disabled_| state
    // hasn't changed yet. RenderThreadImpl::CompositingModeFallbackToSoftware()
    // will lose the context to let us know if it changes.
    return;
  }

  is_gpu_compositing_disabled_ = false;
  context_provider_wrapper_ = nullptr;

  if (context_provider_factory_) {
    // This path should only be used in unit tests.
    auto context_provider = context_provider_factory_.Run();
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
    std::unique_ptr<blink::WebGraphicsContext3DProvider> context_provider;
    context_provider =
        Platform::Current()->CreateSharedOffscreenGraphicsContext3DProvider();
    if (context_provider) {
      context_provider_wrapper_ =
          std::make_unique<WebGraphicsContext3DProviderWrapper>(
              std::move(context_provider));
    }
    gpu_memory_buffer_manager_ =
        Platform::Current()->GetGpuMemoryBufferManager();
  } else {
    // This synchronous round-trip to the main thread is the reason why
    // SharedGpuContext encasulates the context provider: so we only have to do
    // this once per thread.
    base::WaitableEvent waitable_event;
    scoped_refptr<base::SingleThreadTaskRunner> task_runner =
        Thread::MainThread()->GetTaskRunner(MainThreadTaskRunnerRestricted());
    PostCrossThreadTask(
        *task_runner, FROM_HERE,
        CrossThreadBindOnce(
            &CreateContextProviderOnMainThread, only_if_gpu_compositing,
            CrossThreadUnretained(&is_gpu_compositing_disabled_),
            CrossThreadUnretained(&context_provider_wrapper_),
            CrossThreadUnretained(&gpu_memory_buffer_manager_),
            CrossThreadUnretained(&waitable_event)));
    waitable_event.Wait();
    if (context_provider_wrapper_ &&
        !context_provider_wrapper_->ContextProvider()->BindToCurrentSequence())
      context_provider_wrapper_ = nullptr;
  }
}

static void CreateGpuChannelOnMainThread(
    scoped_refptr<gpu::GpuChannelHost>* gpu_channel,
    base::WaitableEvent* waitable_event) {
  DCHECK(IsMainThread());

  *gpu_channel = Platform::Current()->EstablishGpuChannelSync();
  waitable_event->Signal();
}

void SharedGpuContext::CreateSharedImageInterfaceProviderIfNeeded() {
  // If the feature is not enabled, |shared_image_interface_provider_| is always
  // nullptr.
  if (!features::IsCanvasSharedBitmapConversionEnabled()) {
    return;
  }

  // Use the current |shared_image_interface_provider_|.
  if (shared_image_interface_provider_ &&
      shared_image_interface_provider_->SharedImageInterface()) {
    return;
  }

  // Delete and recreate |shared_image_interface_provider_|.
  shared_image_interface_provider_.reset();

  scoped_refptr<gpu::GpuChannelHost> gpu_channel;
  if (IsMainThread()) {
    gpu_channel = Platform::Current()->EstablishGpuChannelSync();
  } else {
    base::WaitableEvent waitable_event;
    scoped_refptr<base::SingleThreadTaskRunner> task_runner =
        Thread::MainThread()->GetTaskRunner(MainThreadTaskRunnerRestricted());
    PostCrossThreadTask(
        *task_runner, FROM_HERE,
        CrossThreadBindOnce(&CreateGpuChannelOnMainThread,
                            CrossThreadUnretained(&gpu_channel),
                            CrossThreadUnretained(&waitable_event)));
    waitable_event.Wait();
  }

  if (!gpu_channel) {
    return;
  }

  auto shared_image_interface = gpu_channel->CreateClientSharedImageInterface();
  if (!shared_image_interface) {
    return;
  }

  shared_image_interface_provider_ =
      std::make_unique<WebGraphicsSharedImageInterfaceProviderImpl>(
          std::move(shared_image_interface));
}

// static
void SharedGpuContext::SetContextProviderFactoryForTesting(
    ContextProviderFactory factory) {
  SharedGpuContext* this_ptr = GetInstanceForCurrentThread();
  DCHECK(!this_ptr->context_provider_wrapper_)
      << this_ptr->context_provider_wrapper_.get();

  this_ptr->context_provider_factory_ = std::move(factory);
}

// static
void SharedGpuContext::Reset() {
  SharedGpuContext* this_ptr = GetInstanceForCurrentThread();
  this_ptr->is_gpu_compositing_disabled_ = false;
  this_ptr->shared_image_interface_provider_.reset();
  this_ptr->context_provider_wrapper_.reset();
  this_ptr->context_provider_factory_.Reset();
  this_ptr->gpu_memory_buffer_manager_ = nullptr;
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

#if BUILDFLAG(IS_ANDROID)
bool SharedGpuContext::MaySupportImageChromium() {
  SharedGpuContext* this_ptr = GetInstanceForCurrentThread();
  this_ptr->CreateContextProviderIfNeeded(/*only_if_gpu_compositing=*/true);
  if (!this_ptr->context_provider_wrapper_) {
    return false;
  }
  const gpu::GpuFeatureInfo& gpu_feature_info =
      this_ptr->context_provider_wrapper_->ContextProvider()
          ->GetGpuFeatureInfo();
  return gpu_feature_info
             .status_values[gpu::GPU_FEATURE_TYPE_ANDROID_SURFACE_CONTROL] ==
         gpu::kGpuFeatureStatusEnabled;
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // blink
