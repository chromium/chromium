// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/semi_realtime_audio_worklet_thread.h"

#include "base/feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/modules/webaudio/audio_worklet_global_scope.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

namespace {

// Use for ref-counting of all SemiRealtimeAudioWorkletThread instances in a
// process. Incremented by the constructor and decremented by destructor.
int ref_count = 0;

void EnsureSharedBackingThread(const ThreadCreationParams& params) {
  DCHECK(IsMainThread());
  DCHECK_EQ(ref_count, 1);
  WorkletThreadHolder<SemiRealtimeAudioWorkletThread>::EnsureInstance(params);
}

}  // namespace

template class WorkletThreadHolder<SemiRealtimeAudioWorkletThread>;

SemiRealtimeAudioWorkletThread::SemiRealtimeAudioWorkletThread(
    WorkerReportingProxy& worker_reporting_proxy)
    : WorkerThread(worker_reporting_proxy) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("audio-worklet"),
               "SemiRealtimeAudioWorklet()");

  DCHECK(IsMainThread());

  ThreadCreationParams params =
      ThreadCreationParams(ThreadType::kSemiRealtimeAudioWorkletThread);

  // Use a higher priority thread only when it is allowed by Finch.
  if (base::FeatureList::IsEnabled(
          features::kAudioWorkletThreadRealtimePriority)) {
    // TODO(crbug.com/1022888): The worklet thread priority is always NORMAL on
    // Linux and Chrome OS regardless of this thread priority setting.
    params.base_thread_type = base::ThreadType::kDisplayCritical;
  } else {
    params.base_thread_type = base::ThreadType::kDefault;
  }

  if (++ref_count == 1) {
    EnsureSharedBackingThread(params);
  }
}

SemiRealtimeAudioWorkletThread::~SemiRealtimeAudioWorkletThread() {
  DCHECK(IsMainThread());
  DCHECK_GT(ref_count, 0);
  if (--ref_count == 0) {
    ClearSharedBackingThread();
  }
}

WorkerBackingThread& SemiRealtimeAudioWorkletThread::GetWorkerBackingThread() {
  return *WorkletThreadHolder<SemiRealtimeAudioWorkletThread>::GetInstance()
      ->GetThread();
}

void SemiRealtimeAudioWorkletThread::ClearSharedBackingThread() {
  DCHECK(IsMainThread());
  CHECK_EQ(ref_count, 0);
  WorkletThreadHolder<SemiRealtimeAudioWorkletThread>::ClearInstance();
}

WorkerOrWorkletGlobalScope*
SemiRealtimeAudioWorkletThread::CreateWorkerGlobalScope(
    std::unique_ptr<GlobalScopeCreationParams> creation_params) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("audio-worklet"),
               "SemiRealtimeAudioWorkletThread::CreateWorkerGlobalScope");
  return MakeGarbageCollected<AudioWorkletGlobalScope>(
      std::move(creation_params), this);
}

}  // namespace blink
