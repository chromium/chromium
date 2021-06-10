// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/csspaint/native_paint_worklet.h"

#include "base/synchronization/waitable_event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_impl.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/workers/worker_backing_thread_startup_data.h"
#include "third_party/blink/renderer/modules/csspaint/native_paint_worklet_proxy_client.h"
#include "third_party/blink/renderer/modules/csspaint/paint_worklet_id_generator.h"
#include "third_party/blink/renderer/platform/graphics/paint_worklet_paint_dispatcher.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_or_worker_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
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
  ExecutionContext* context = local_root.DomWindow();
  FrameOrWorkerScheduler* scheduler =
      context ? context->GetScheduler() : nullptr;
  // TODO(crbug.com/1143407): We don't need this thread if we can make the
  // compositor thread support GC.
  ThreadCreationParams params(ThreadType::kAnimationAndPaintWorkletThread);
  worker_backing_thread_ = std::make_unique<WorkerBackingThread>(
      params.SetFrameOrWorkerScheduler(scheduler));
  auto startup_data = WorkerBackingThreadStartupData::CreateDefault();
  PostCrossThreadTask(
      *worker_backing_thread_->BackingThread().GetTaskRunner(), FROM_HERE,
      CrossThreadBindOnce(&WorkerBackingThread::InitializeOnBackingThread,
                          CrossThreadUnretained(worker_backing_thread_.get()),
                          startup_data));
}

void NativePaintWorklet::RegisterProxyClient(
    NativePaintWorkletProxyClient* client) {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      worker_backing_thread_->BackingThread().GetTaskRunner();
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
  base::WaitableEvent waitable_event;
  PostCrossThreadTask(*worker_backing_thread_->BackingThread().GetTaskRunner(),
                      FROM_HERE,
                      CrossThreadBindOnce(
                          [](WorkerBackingThread* worker_backing_thread,
                             base::WaitableEvent* waitable_event) {
                            worker_backing_thread->ShutdownOnBackingThread();
                            waitable_event->Signal();
                          },
                          CrossThreadUnretained(worker_backing_thread_.get()),
                          CrossThreadUnretained(&waitable_event)));
  waitable_event.Wait();
  worker_backing_thread_.reset();
}

}  // namespace blink
