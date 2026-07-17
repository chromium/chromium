/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/timing/worker_performance.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/timing/animation_frame_timing_info.h"
#include "third_party/blink/renderer/core/timing/performance_long_animation_frame_timing.h"
#include "third_party/blink/renderer/core/workers/dedicated_worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"

namespace blink {

WorkerPerformance::WorkerPerformance(WorkerGlobalScope* context)
    : Performance(context->TimeOrigin(),
                  context->CrossOriginIsolatedCapability(),
                  context->GetTaskRunner(TaskType::kPerformanceTimeline),
                  context),
      execution_context_(context) {}

void WorkerPerformance::QueueLongAnimationFrameTiming(
    AnimationFrameTimingInfo* info) {
  CHECK(RuntimeEnabledFeatures::LongAnimationFrameWorkerEnabled());
  PerformanceLongAnimationFrameTiming* entry =
      PerformanceLongAnimationFrameTiming::Create(
          info, time_origin_, cross_origin_isolated_capability_,
          execution_context_, /*paint_timing_info=*/std::nullopt,
          /*navigation_id=*/0);
  if (!IsLongAnimationFrameBufferFull()) {
    InsertEntryIntoSortedBuffer(long_animation_frame_buffer_, *entry);
  }
  NotifyObserversOfEntry(*entry);
}

void WorkerPerformance::Trace(Visitor* visitor) const {
  visitor->Trace(execution_context_);
  Performance::Trace(visitor);
}

}  // namespace blink
