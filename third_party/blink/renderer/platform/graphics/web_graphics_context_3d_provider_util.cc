// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_util.h"

#include "base/synchronization/waitable_event.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
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
  Platform::GraphicsInfo* gl_info;
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

void CreateWebGPUGraphicsContextOnMainThread(
    const KURL& url,
    base::WaitableEvent* waitable_event,
    std::unique_ptr<WebGraphicsContext3DProvider>* created_context_provider) {
  DCHECK(IsMainThread());
  *created_context_provider =
      Platform::Current()->CreateWebGPUGraphicsContext3DProvider(url);
  waitable_event->Signal();
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

std::unique_ptr<WebGraphicsContext3DProvider>
CreateWebGPUGraphicsContext3DProvider(const KURL& url) {
  if (IsMainThread()) {
    return Platform::Current()->CreateWebGPUGraphicsContext3DProvider(url);
  } else {
    base::WaitableEvent waitable_event;
    std::unique_ptr<WebGraphicsContext3DProvider> created_context_provider;
    PostCrossThreadTask(
        *Thread::MainThread()->GetTaskRunner(
            AccessMainThreadForWebGraphicsContext3DProvider()),
        FROM_HERE,
        CrossThreadBindOnce(&CreateWebGPUGraphicsContextOnMainThread, url,
                            CrossThreadUnretained(&waitable_event),
                            CrossThreadUnretained(&created_context_provider)));

    waitable_event.Wait();
    return created_context_provider;
  }
}

}  // namespace blink
