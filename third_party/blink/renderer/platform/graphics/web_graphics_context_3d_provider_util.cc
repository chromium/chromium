// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_util.h"

#include "base/memory/raw_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/platform/web_url.h"
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

struct ContextProviderCreationInfo {
  // Inputs.
  Platform::ContextAttributes context_attributes;
  raw_ptr<Platform::GraphicsInfo> gl_info;
  KURL url;
  // Outputs.
  std::unique_ptr<WebGraphicsContext3DProvider> created_context_provider;
};

void CreateOffscreenGraphicsContextOnMainThread(
    ContextProviderCreationInfo* creation_info,
    base::WaitableEvent* waitable_event) {
  DCHECK(IsMainThread());
  // The gpu compositing mode is snapshotted in the GraphicsInfo when
  // making the context. The context will be lost if the mode changes.
  creation_info->created_context_provider =
      Platform::Current()->CreateOffscreenGraphicsContext3DProvider(
          creation_info->context_attributes, creation_info->url,
          creation_info->gl_info);
  waitable_event->Signal();
}

void CreateWebGPUGraphicsContextOnMainThreadAsync(
    KURL url,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    CrossThreadOnceFunction<void(std::unique_ptr<WebGraphicsContext3DProvider>)>
        callback) {
  DCHECK(IsMainThread());
  PostCrossThreadTask(
      *task_runner, FROM_HERE,
      CrossThreadBindOnce(
          std::move(callback),
          Platform::Current()->CreateWebGPUGraphicsContext3DProvider(url)));
}

}  // namespace

std::unique_ptr<WebGraphicsContext3DProvider>
CreateOffscreenGraphicsContext3DProvider(
    Platform::ContextAttributes context_attributes,
    Platform::GraphicsInfo* gl_info,
    const KURL& url) {
  if (IsMainThread()) {
    return Platform::Current()->CreateOffscreenGraphicsContext3DProvider(
        context_attributes, url, gl_info);
  } else {
    base::WaitableEvent waitable_event;
    ContextProviderCreationInfo creation_info;
    creation_info.context_attributes = context_attributes;
    creation_info.gl_info = gl_info;
    creation_info.url = url;
    PostCrossThreadTask(
        *Thread::MainThread()->GetTaskRunner(
            AccessMainThreadForWebGraphicsContext3DProvider()),
        FROM_HERE,
        CrossThreadBindOnce(&CreateOffscreenGraphicsContextOnMainThread,
                            CrossThreadUnretained(&creation_info),
                            CrossThreadUnretained(&waitable_event)));
    waitable_event.Wait();
    return std::move(creation_info.created_context_provider);
  }
}

void CreateWebGPUGraphicsContext3DProviderAsync(
    const KURL& url,
    scoped_refptr<base::SingleThreadTaskRunner> current_thread_task_runner,
    WTF::CrossThreadOnceFunction<
        void(std::unique_ptr<WebGraphicsContext3DProvider>)> callback) {
  if (IsMainThread()) {
    Platform::Current()->CreateWebGPUGraphicsContext3DProviderAsync(
        url, ConvertToBaseOnceCallback(std::move(callback)));
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
                            current_thread_task_runner, std::move(callback)));
  }
}

}  // namespace blink
