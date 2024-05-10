// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint_worklet_paint_dispatcher.h"

#include <utility>

#include "base/barrier_closure.h"
#include "base/containers/contains.h"
#include "base/functional/callback_helpers.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/scheduler/public/non_main_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_std.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

// static
std::unique_ptr<PlatformPaintWorkletLayerPainter>
PaintWorkletPaintDispatcher::CreateCompositorThreadPainter(
    base::WeakPtr<PaintWorkletPaintDispatcher>* paint_dispatcher) {
  DCHECK(IsMainThread());
  auto dispatcher = std::make_unique<PaintWorkletPaintDispatcher>();
  *paint_dispatcher = dispatcher->GetWeakPtr();

  return std::make_unique<PlatformPaintWorkletLayerPainter>(
      std::move(dispatcher));
}

PaintWorkletPaintDispatcher::PaintWorkletPaintDispatcher() {
  // PaintWorkletPaintDispatcher is created on the main thread but used on the
  // compositor, so detach the sequence checker until a call is received.
  DCHECK(IsMainThread());
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

void PaintWorkletPaintDispatcher::RegisterPaintWorkletPainter(
    PaintWorkletPainter* painter,
    scoped_refptr<base::SingleThreadTaskRunner> painter_runner) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("cc",
               "PaintWorkletPaintDispatcher::RegisterPaintWorkletPainter");

  int worklet_id = painter->GetWorkletId();
  DCHECK(!base::Contains(painter_map_, worklet_id));
  painter_map_.insert(worklet_id, std::make_pair(painter, painter_runner));
}

void PaintWorkletPaintDispatcher::UnregisterPaintWorkletPainter(
    int worklet_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("cc",
               "PaintWorkletPaintDispatcher::"
               "UnregisterPaintWorkletPainter");
  DCHECK(base::Contains(painter_map_, worklet_id));
  painter_map_.erase(worklet_id);
}

void PaintWorkletPaintDispatcher::DispatchWorklets(
    cc::PaintWorkletJobMap worklet_job_map,
    PlatformPaintWorkletLayerPainter::DoneCallback done_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("cc", "PaintWorkletPaintDispatcher::DispatchWorklets");

  // We must be called with a valid callback to guarantee our internal state.
  DCHECK(!done_callback.is_null());

  // Dispatching to the worklets is an asynchronous process, but there should
  // only be one dispatch going on at once. We store the completion callback and
  // the PaintWorklet job map in the class during the dispatch, then clear them
  // when we get results (see AsyncPaintDone).
  DCHECK(on_async_paint_complete_.is_null());
  on_async_paint_complete_ = std::move(done_callback);
  ongoing_jobs_ = std::move(worklet_job_map);

  scoped_refptr<base::SingleThreadTaskRunner> runner =
      GetCompositorTaskRunner();
  WTF::CrossThreadClosure on_done = CrossThreadBindRepeating(
      [](base::WeakPtr<PaintWorkletPaintDispatcher> dispatcher,
         scoped_refptr<base::SingleThreadTaskRunner> runner) {
        PostCrossThreadTask(
            *runner, FROM_HERE,
            CrossThreadBindOnce(&PaintWorkletPaintDispatcher::AsyncPaintDone,
                                dispatcher));
      },
      weak_factory_.GetWeakPtr(), std::move(runner));

  // Use a base::RepeatingClosure to make sure that AsyncPaintDone is only
  // called once, once all the worklets are done. If there are no inputs
  // specified, base::RepeatingClosure will trigger immediately and so the
  // callback will still happen.
  base::RepeatingClosure repeating_on_done = base::BarrierClosure(
      ongoing_jobs_.size(), ConvertToBaseRepeatingCallback(std::move(on_done)));

  // Now dispatch the calls to the registered painters. For each input, we match
  // the id to a registered worklet and dispatch a cross-thread call to it,
  // using the above-created base::RepeatingClosure.
  for (auto& job : ongoing_jobs_) {
    int worklet_id = job.first;
    scoped_refptr<cc::PaintWorkletJobVector> jobs = job.second;

    // Wrap the barrier closure in a ScopedClosureRunner to guarantee it runs
    // even if there is no matching worklet or the posted task does not run.
    auto on_done_runner =
        std::make_unique<base::ScopedClosureRunner>(repeating_on_done);

    auto it = painter_map_.find(worklet_id);
    if (it == painter_map_.end())
      continue;

    PaintWorkletPainter* painter = it->value.first;
    scoped_refptr<base::SingleThreadTaskRunner> task_runner = it->value.second;

    if (task_runner) {
      DCHECK(!task_runner->BelongsToCurrentThread());

      PostCrossThreadTask(
          *task_runner, FROM_HERE,
          CrossThreadBindOnce(
              [](PaintWorkletPainter* painter,
                 scoped_refptr<cc::PaintWorkletJobVector> jobs,
                 std::unique_ptr<base::ScopedClosureRunner> on_done_runner) {
                for (cc::PaintWorkletJob& job : jobs->data) {
                  job.SetOutput(painter->Paint(
                      job.input().get(), job.GetAnimatedPropertyValues()));
                }
                on_done_runner->RunAndReset();
              },
              WrapCrossThreadPersistent(painter), std::move(jobs),
              std::move(on_done_runner)));
    } else {
      // A native paint worklet can run on the compsitor thread provided it does
      // not require garbage collection.
      for (cc::PaintWorkletJob& native_job : jobs->data) {
        native_job.SetOutput(painter->Paint(
            native_job.input().get(), native_job.GetAnimatedPropertyValues()));
      }
      on_done_runner->RunAndReset();
    }
  }
}

bool PaintWorkletPaintDispatcher::HasOngoingDispatch() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return !on_async_paint_complete_.is_null();
}

void PaintWorkletPaintDispatcher::AsyncPaintDone() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("cc", "PaintWorkletPaintDispatcher::AsyncPaintDone");
  std::move(on_async_paint_complete_).Run(std::move(ongoing_jobs_));
}

scoped_refptr<base::SingleThreadTaskRunner>
PaintWorkletPaintDispatcher::GetCompositorTaskRunner() {
  DCHECK(Thread::CompositorThread());
  DCHECK(Thread::CompositorThread()->IsCurrentThread());
  return Thread::CompositorThread()->GetTaskRunner();
}

}  // namespace blink
