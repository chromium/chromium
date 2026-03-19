// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"

#include "base/command_line.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/ipc/client/client_shared_image_interface.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_graphics_context_3d_provider.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgraphics_shared_image_interface_provider_impl.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

namespace {

std::optional<bool> g_use_mappable_shared_images_for_canvas_2d_for_testing;
std::optional<bool> g_low_latency_usage_supported_for_canvas_2d_for_testing;
std::optional<bool> g_use_overlays_for_webgl_for_testing;
std::optional<bool> g_low_latency_usage_supported_for_webgl_for_testing;

#if BUILDFLAG(IS_APPLE)
bool IsDelegatedCompositingEnabled() {
  static const bool backed_by_io_surface =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableGpuMemoryBufferCompositorResources);
  return backed_by_io_surface;
}
#endif

#if BUILDFLAG(IS_ANDROID)
bool IsSurfaceControlEnabled() {
  return ::features::IsAndroidSurfaceControlEnabled();
}
#endif

}  // namespace

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

base::WeakPtr<WebGraphicsContext3DProviderWrapper>
SharedGpuContext::GetExistingContextProviderWrapper() {
  SharedGpuContext* this_ptr = GetInstanceForCurrentThread();
  if (!this_ptr->context_provider_wrapper_) {
    return nullptr;
  }
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

static void CreateContextProviderOnMainThread(
    bool only_if_gpu_compositing,
    bool* gpu_compositing_disabled,
    std::unique_ptr<WebGraphicsContext3DProviderWrapper>* wrapper,
    base::WaitableEvent* waitable_event) {
  DCHECK(IsMainThread());

  *gpu_compositing_disabled = Platform::Current()->IsGpuCompositingDisabled();
  if (*gpu_compositing_disabled && only_if_gpu_compositing) {
    waitable_event->Signal();
    return;
  }

  auto context_provider =
      Platform::Current()->CreateRasterGraphicsContextProvider(
          WebURL(), Platform::RasterContextType::kSharedGpuContextWorker);
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
      !context_provider_wrapper_->ContextProvider().IsContextLost()) {
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
            CrossThreadUnretained(&waitable_event)));
    waitable_event.Wait();
    if (context_provider_wrapper_ &&
        !context_provider_wrapper_->ContextProvider().BindToCurrentSequence()) {
      context_provider_wrapper_ = nullptr;
    }
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

  shared_image_interface_provider_ =
      WebGraphicsSharedImageInterfaceProviderImpl::TryCreate(
          std::move(gpu_channel));
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
  g_use_mappable_shared_images_for_canvas_2d_for_testing.reset();
  g_low_latency_usage_supported_for_canvas_2d_for_testing.reset();
  g_use_overlays_for_webgl_for_testing.reset();
  g_low_latency_usage_supported_for_webgl_for_testing.reset();
}

bool SharedGpuContext::IsValidWithoutRestoringForTesting() {
  SharedGpuContext* this_ptr = GetInstanceForCurrentThread();
  if (!this_ptr->context_provider_wrapper_)
    return false;
  auto* gl_context =
      this_ptr->context_provider_wrapper_->ContextProvider().ContextGL();

  if (gl_context) {
    return gl_context->GetGraphicsResetStatusKHR() == GL_NO_ERROR;
  }

  auto* raster_interface =
      this_ptr->context_provider_wrapper_->ContextProvider().RasterInterface();
  CHECK(raster_interface);
  return raster_interface->GetGraphicsResetStatusKHR() == GL_NO_ERROR;
}

bool SharedGpuContext::AllowSoftwareToAcceleratedCanvasUpgrade() {
  SharedGpuContext* this_ptr = GetInstanceForCurrentThread();
  bool only_if_gpu_compositing = false;
  this_ptr->CreateContextProviderIfNeeded(only_if_gpu_compositing);
  if (!this_ptr->context_provider_wrapper_)
    return false;
  return !this_ptr->context_provider_wrapper_->ContextProvider()
              .GetGpuFeatureInfo()
              .IsWorkaroundEnabled(
                  gpu::DISABLE_SOFTWARE_TO_ACCELERATED_CANVAS_UPGRADE);
}

bool SharedGpuContext::UseMappableSharedImagesForCanvas2D() {
  if (g_use_mappable_shared_images_for_canvas_2d_for_testing) {
    return g_use_mappable_shared_images_for_canvas_2d_for_testing.value();
  }

#if BUILDFLAG(IS_APPLE)
  // Native mappable SharedImage is always available on Apple platforms. If
  // delegated compositing is enabled, we exploit this fact to use mappable
  // SharedImages as the backing for 2D canvases. Note that we know that in
  // this case they will have SCANOUT usage added as well (see the method
  // below).
  return IsDelegatedCompositingEnabled();
#else
  return false;
#endif
}

void SharedGpuContext::SetUseMappableSharedImagesForCanvas2DForTesting(
    bool enable) {
  g_use_mappable_shared_images_for_canvas_2d_for_testing = enable;
}

bool SharedGpuContext::UseOverlaysForCanvas2D() {
#if BUILDFLAG(IS_APPLE)
  // Delegated compositing on Apple platforms is all-or-nothing as there is no
  // API for partial delegation. Hence, if delegated compositing is enabled, we
  // want 2D canvases to end up in overlays.
  // We could consider extending this to other platforms that use delegated
  // compositing (e.g., Windows).
  return IsDelegatedCompositingEnabled();
#else
  return false;
#endif
}

void SharedGpuContext::SetLowLatencyUsageSupportedForCanvas2DForTesting(
    bool enable) {
  g_low_latency_usage_supported_for_canvas_2d_for_testing = enable;
}

bool SharedGpuContext::LowLatencyUsageSupportedForCanvas2D(
    RasterMode raster_mode) {
  if (g_low_latency_usage_supported_for_canvas_2d_for_testing) {
    return g_low_latency_usage_supported_for_canvas_2d_for_testing.value();
  }

  // Concurrent read/write only makes sense if raster writes are happening via
  // the GPU.
  if (raster_mode == RasterMode::kCPU) {
    return false;
  }

#if BUILDFLAG(IS_WIN)
  // Low-latency usages are supported on Windows if it's possible to back
  // SharedImages by the D3D swapchain.
  return ContextProviderWrapper()
      ->ContextProvider()
      .SharedImageInterface()
      ->GetCapabilities()
      .shared_image_swap_chain;
#elif BUILDFLAG(IS_ANDROID)
  // Low-latency usage on Android is possible only with SurfaceControl.
  return IsSurfaceControlEnabled() &&
         base::FeatureList::IsEnabled(
             features::kLowLatencyUsageSupportedForCanvas2D);
#elif BUILDFLAG(IS_CHROMEOS)
  // Low-latency usage is always supported for Canvas2D on ChromeOS.
  return true;
#else
  // NOTE: crbug.com/41435781 would need to be resolved in order to support
  // low-latency usage on Mac (currently setting the desynchronized attribute
  // on a canvas is a no-op on Mac). If/once that bug is resolved, determine
  // whether this method can then return true on Apple if
  // IsDelegatedCompositingEnabled() holds.
  return false;
#endif
}

bool SharedGpuContext::LowLatencyUsageSupportedForWebGL(
    gpu::SharedImageInterface* sii) {
  if (g_low_latency_usage_supported_for_webgl_for_testing) {
    return g_low_latency_usage_supported_for_webgl_for_testing.value();
  }

#if BUILDFLAG(IS_ANDROID)
  // Low-latency usage on Android is possible only with SurfaceControl.
  return IsSurfaceControlEnabled() &&
         base::FeatureList::IsEnabled(
             features::kLowLatencyUsageSupportedForWebGL);
#elif BUILDFLAG(IS_CHROMEOS)
  // Whether WebGL canvases should be given low-latency usage is specified on a
  // per-board basis by passing (or not) the relevant command-line flag.
  static const bool enable_web_gl_image_chromium =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          blink::switches::kEnableWebGLImageChromium);
  return enable_web_gl_image_chromium;
#elif BUILDFLAG(IS_WIN)
  return sii && sii->GetCapabilities().shared_image_swap_chain;
#else
  // NOTE: crbug.com/41435781 would need to be resolved in order to support
  // low-latency usage on Mac (currently setting the desynchronized attribute
  // on a canvas is a no-op on Mac). If/once that bug is resolved, determine
  // whether this method can then return true on Apple if
  // IsDelegatedCompositingEnabled() holds.
  return false;
#endif
}

bool SharedGpuContext::UseOverlaysForWebGL() {
  if (g_use_overlays_for_webgl_for_testing) {
    return g_use_overlays_for_webgl_for_testing.value();
  }

#if BUILDFLAG(IS_APPLE)
  // Delegated compositing on Apple platforms is all-or-nothing as there is no
  // API for partial delegation. Hence, if delegated compositing is enabled, we
  // want WebGL canvases to end up in overlays.
  // We could consider extending this to other platforms that use delegated
  // compositing (e.g., Windows).
  return IsDelegatedCompositingEnabled();
#elif BUILDFLAG(IS_CHROMEOS)
  // Whether WebGL canvases should be placed in overlays is specified on a
  // per-board basis by passing (or not) the relevant command-line flag.
  static const bool enable_web_gl_image_chromium =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          blink::switches::kEnableWebGLImageChromium);
  return enable_web_gl_image_chromium;
#else
  return false;
#endif
}

void SharedGpuContext::SetUseOverlaysForWebGLForTesting(bool enable) {
  g_use_overlays_for_webgl_for_testing = enable;
}

void SharedGpuContext::SetLowLatencyUsageSupportedForWebGLForTesting(
    bool enable) {
  g_low_latency_usage_supported_for_webgl_for_testing = enable;
}

}  // namespace blink
