// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_util.h"

#include "base/memory/raw_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "gpu/command_buffer/client/context_support.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_handle.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_std.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

// Define a function that is allowed to access MainThreadTaskRunnerRestricted.
MainThreadTaskRunnerRestricted
AccessMainThreadForWebGraphicsContext3DProvider() {
  return {};
}

namespace {

void CreateWebGPUGraphicsContextOnMainThreadAsync(
    KURL url,
    Platform::WebGPUReplyThread reply_thread,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    CrossThreadOnceFunction<void(std::unique_ptr<WebGraphicsContext3DProvider>)>
        callback) {
  DCHECK(IsMainThread());
  PostCrossThreadTask(
      *task_runner, FROM_HERE,
      CrossThreadBindOnce(
          std::move(callback),
          Platform::Current()->CreateWebGPUGraphicsContext3DProvider(
              url, reply_thread)));
}

}  // namespace

std::unique_ptr<WebGraphicsContext3DProvider>
CreateRasterGraphicsContextProvider(const KURL& url,
                                    Platform::RasterContextType context_type) {
  if (IsMainThread()) {
    return Platform::Current()->CreateRasterGraphicsContextProvider(
        url, context_type);
  } else {
    base::WaitableEvent waitable_event;
    std::unique_ptr<WebGraphicsContext3DProvider> created_context_provider;
    PostCrossThreadTask(
        *Thread::MainThread()->GetTaskRunner(
            AccessMainThreadForWebGraphicsContext3DProvider()),
        FROM_HERE,
        CrossThreadBindOnce(
            [](const KURL& url, Platform::RasterContextType context_type,
               std::unique_ptr<WebGraphicsContext3DProvider>* out_provider,
               base::WaitableEvent* waitable_event) {
              DCHECK(IsMainThread());
              // The gpu compositing mode is snapshotted in the GraphicsInfo
              // when making the context. The context will be lost if the mode
              // changes.
              *out_provider =
                  Platform::Current()->CreateRasterGraphicsContextProvider(
                      url, context_type);
              waitable_event->Signal();
            },
            url, context_type, CrossThreadUnretained(&created_context_provider),
            CrossThreadUnretained(&waitable_event)));
    waitable_event.Wait();
    return created_context_provider;
  }
}

std::unique_ptr<WebGraphicsContext3DProvider>
CreateWebGLGraphicsContextProvider(bool prefer_low_power_gpu,
                                   bool fail_if_major_performance_caveat,
                                   Platform::WebGLContextType context_type,
                                   Platform::WebGLContextInfo* gl_info,
                                   const KURL& url) {
  if (IsMainThread()) {
    return Platform::Current()->CreateWebGLGraphicsContextProvider(
        prefer_low_power_gpu, fail_if_major_performance_caveat, context_type,
        url, gl_info);
  } else {
    base::WaitableEvent waitable_event;
    std::unique_ptr<WebGraphicsContext3DProvider> created_context_provider;
    PostCrossThreadTask(
        *Thread::MainThread()->GetTaskRunner(
            AccessMainThreadForWebGraphicsContext3DProvider()),
        FROM_HERE,
        CrossThreadBindOnce(
            [](bool prefer_low_power_gpu, bool fail_if_major_performance_caveat,
               Platform::WebGLContextType context_type,
               Platform::WebGLContextInfo* gl_info, const KURL& url,
               std::unique_ptr<WebGraphicsContext3DProvider>* out_provider,
               base::WaitableEvent* waitable_event) {
              DCHECK(IsMainThread());
              // The gpu compositing mode is snapshotted in the WebGLContextInfo
              // when making the context. The context will be lost if the mode
              // changes.
              *out_provider =
                  Platform::Current()->CreateWebGLGraphicsContextProvider(
                      prefer_low_power_gpu, fail_if_major_performance_caveat,
                      context_type, url, gl_info);
              waitable_event->Signal();
            },
            prefer_low_power_gpu, fail_if_major_performance_caveat,
            context_type, CrossThreadUnretained(gl_info), url,
            CrossThreadUnretained(&created_context_provider),
            CrossThreadUnretained(&waitable_event)));
    waitable_event.Wait();
    return created_context_provider;
  }
}

void CreateWebGPUGraphicsContext3DProviderAsync(
    const KURL& url,
    Platform::WebGPUReplyThread reply_thread,
    scoped_refptr<base::SingleThreadTaskRunner> current_thread_task_runner,
    CrossThreadOnceFunction<void(std::unique_ptr<WebGraphicsContext3DProvider>)>
        callback) {
  if (IsMainThread()) {
    Platform::Current()->CreateWebGPUGraphicsContext3DProviderAsync(
        url, reply_thread, ConvertToBaseOnceCallback(std::move(callback)));
  } else {
    // Posts a task to the main thread to create context provider
    // because the current RendererBlinkPlatformImpl and viz::Gpu
    // APIs allow to create it only on the main thread.
    // When it is created, posts it back to the current thread
    // and call the callback with it.
    // TODO(takahiro): Directly create context provider on Workers threads
    //                 if RendererBlinkPlatformImpl and viz::Gpu will start to
    //                 allow the context provider creation on Workers.
    PostCrossThreadTask(
        *Thread::MainThread()->GetTaskRunner(
            AccessMainThreadForWebGraphicsContext3DProvider()),
        FROM_HERE,
        CrossThreadBindOnce(&CreateWebGPUGraphicsContextOnMainThreadAsync, url,
                            reply_thread, current_thread_task_runner,
                            std::move(callback)));
  }
}

void SetAggressivelyFreeSharedGpuContextResourcesIfPossible(bool value) {
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper =
      SharedGpuContext::GetExistingContextProviderWrapper();

  if (!context_provider_wrapper) {
    return;
  }

  gpu::ContextSupport* context_support =
      context_provider_wrapper->ContextProvider().ContextSupport();
  if (!context_support) {
    return;
  }

  context_support->SetAggressivelyFreeResources(value);
}

}  // namespace blink
