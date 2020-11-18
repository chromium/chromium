// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/csspaint/native_paint_worklet.h"

#include "third_party/blink/renderer/core/frame/web_frame_widget_base.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/modules/csspaint/native_paint_worklet_proxy_client.h"
#include "third_party/blink/renderer/modules/csspaint/paint_worklet_id_generator.h"
#include "third_party/blink/renderer/platform/graphics/paint_worklet_paint_dispatcher.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

NativePaintWorklet::NativePaintWorklet(LocalFrame& local_root)
    : worklet_id_(PaintWorkletIdGenerator::NextId()) {
  DCHECK(local_root.IsLocalRoot());
  paint_dispatcher_ =
      WebLocalFrameImpl::FromFrame(local_root)
          ->FrameWidgetImpl()
          ->EnsureCompositorPaintDispatcher(&compositor_host_queue_);
  DCHECK(IsMainThread());
  ThreadCreationParams params(ThreadType::kDedicatedWorkerThread);
  // TODO(crbug.com/1143407): We don't need this thread if we can make the
  // compositor thread support GC.
  worker_thread_ = Thread::CreateThread(params.SetSupportsGC(true));
}

void NativePaintWorklet::RegisterProxyClient(
    NativePaintWorkletProxyClient* client) {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      worker_thread_->GetTaskRunner();
  // At this moment, we are in the paint phase which is before commit, we queue
  // a task to the compositor thread to register the |paint_dispatcher_|. When
  // compositor schedules the actual paint job (PaintWorkletPainter::Paint),
  // which is after commit, the |paint_dispatcher_| should have been registerted
  // and ready to use.
  PostCrossThreadTask(
      *compositor_host_queue_, FROM_HERE,
      CrossThreadBindOnce(
          &PaintWorkletPaintDispatcher::RegisterPaintWorkletPainter,
          paint_dispatcher_, WrapCrossThreadPersistent(client), task_runner));
}

void NativePaintWorklet::UnregisterProxyClient() {
  PostCrossThreadTask(
      *compositor_host_queue_, FROM_HERE,
      CrossThreadBindOnce(
          &PaintWorkletPaintDispatcher::UnregisterPaintWorkletPainter,
          paint_dispatcher_, worklet_id_));
}

}  // namespace blink
