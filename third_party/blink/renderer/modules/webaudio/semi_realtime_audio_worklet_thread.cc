// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/semi_realtime_audio_worklet_thread.h"

#include "third_party/blink/renderer/modules/webaudio/audio_worklet_global_scope.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

template class WorkletThreadHolder<SemiRealtimeAudioWorkletThread>;

int SemiRealtimeAudioWorkletThread::s_ref_count_ = 0;

SemiRealtimeAudioWorkletThread::SemiRealtimeAudioWorkletThread(
    WorkerReportingProxy& worker_reporting_proxy)
    : WorkerThread(worker_reporting_proxy) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("audio-worklet"),
               "SemiRealtimeAudioWorklet()");

  DCHECK(IsMainThread());

  ThreadCreationParams params =
      ThreadCreationParams(ThreadType::kSemiRealtimeAudioWorkletThread);

  // TODO(crbug.com/1022888): The worklet thread priority is always NORMAL
  // on OS_LINUX and OS_CHROMEOS regardless of this thread priority setting.
  params.thread_priority = base::ThreadPriority::DISPLAY;

  if (++s_ref_count_ == 1)
    EnsureSharedBackingThread(params);
}

SemiRealtimeAudioWorkletThread::~SemiRealtimeAudioWorkletThread() {
  DCHECK(IsMainThread());
  if (--s_ref_count_ == 0)
    ClearSharedBackingThread();
}

WorkerBackingThread& SemiRealtimeAudioWorkletThread::GetWorkerBackingThread() {
  return *WorkletThreadHolder<SemiRealtimeAudioWorkletThread>::GetInstance()
      ->GetThread();
}

void SemiRealtimeAudioWorkletThread::EnsureSharedBackingThread(
    const ThreadCreationParams& params) {
  DCHECK(IsMainThread());
  WorkletThreadHolder<SemiRealtimeAudioWorkletThread>::EnsureInstance(params);
}

void SemiRealtimeAudioWorkletThread::ClearSharedBackingThread() {
  DCHECK(IsMainThread());
  CHECK_EQ(s_ref_count_, 0);
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
