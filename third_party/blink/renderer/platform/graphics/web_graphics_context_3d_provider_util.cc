// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_util.h"

#include "base/synchronization/waitable_event.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

struct ContextProviderCreationInfo {
  // Inputs.
  Platform::ContextAttributes context_attributes;
  Platform::GraphicsInfo* gl_info;
  KURL url;
  // Outputs.
  std::unique_ptr<WebGraphicsContext3DProvider> created_context_provider;
  bool* using_gpu_compositing;
};

static void CreateContextProviderOnMainThread(
    ContextProviderCreationInfo* creation_info,
    base::WaitableEvent* waitable_event) {
  DCHECK(IsMainThread());
  // Ask for gpu compositing mode when making the context. The context will be
  // lost if the mode changes.
  *creation_info->using_gpu_compositing =
      !Platform::Current()->IsGpuCompositingDisabled();
  creation_info->created_context_provider =
      Platform::Current()->CreateOffscreenGraphicsContext3DProvider(
          creation_info->context_attributes, creation_info->url,
          creation_info->gl_info);
  waitable_event->Signal();
}

std::unique_ptr<WebGraphicsContext3DProvider>
CreateContextProviderOnWorkerThread(
    Platform::ContextAttributes context_attributes,
    Platform::GraphicsInfo* gl_info,
    bool* using_gpu_compositing,
    const KURL& url) {
  base::WaitableEvent waitable_event;
  ContextProviderCreationInfo creation_info;
  creation_info.context_attributes = context_attributes;
  creation_info.gl_info = gl_info;
  creation_info.url = url.Copy();
  creation_info.using_gpu_compositing = using_gpu_compositing;
  PostCrossThreadTask(
      *Thread::MainThread()->GetTaskRunner(), FROM_HERE,
      CrossThreadBindOnce(&CreateContextProviderOnMainThread,
                          CrossThreadUnretained(&creation_info),
                          CrossThreadUnretained(&waitable_event)));
  waitable_event.Wait();
  return std::move(creation_info.created_context_provider);
}

}  // namespace blink
