// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/realtime_audio_worklet_thread.h"

#include "third_party/blink/renderer/modules/webaudio/audio_worklet_global_scope.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

template class WorkletThreadHolder<RealtimeAudioWorkletThread>;

int RealtimeAudioWorkletThread::s_ref_count_ = 0;

RealtimeAudioWorkletThread::RealtimeAudioWorkletThread(
    WorkerReportingProxy& worker_reporting_proxy)
    : WorkerThread(worker_reporting_proxy) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("audio-worklet"),
               "RealtimeAudioWorkletThread()");

  DCHECK(IsMainThread());

  ThreadCreationParams params =
      ThreadCreationParams(ThreadType::kRealtimeAudioWorkletThread);

  // TODO(crbug.com/1022888): The worklet thread priority is always NORMAL
  // on OS_LINUX and OS_CHROMEOS regardless of the thread priority setting.
  params.thread_priority = base::ThreadPriority::REALTIME_AUDIO;

  if (++s_ref_count_ == 1)
    EnsureSharedBackingThread(params);
}

RealtimeAudioWorkletThread::~RealtimeAudioWorkletThread() {
  DCHECK(IsMainThread());
  if (--s_ref_count_ == 0)
    ClearSharedBackingThread();
}

WorkerBackingThread& RealtimeAudioWorkletThread::GetWorkerBackingThread() {
  return *WorkletThreadHolder<RealtimeAudioWorkletThread>::GetInstance()
      ->GetThread();
}

void RealtimeAudioWorkletThread::EnsureSharedBackingThread(
    const ThreadCreationParams& params) {
  DCHECK(IsMainThread());
  WorkletThreadHolder<RealtimeAudioWorkletThread>::EnsureInstance(params);
}

void RealtimeAudioWorkletThread::ClearSharedBackingThread() {
  DCHECK(IsMainThread());
  CHECK_EQ(s_ref_count_, 0);
  WorkletThreadHolder<RealtimeAudioWorkletThread>::ClearInstance();
}

WorkerOrWorkletGlobalScope* RealtimeAudioWorkletThread::CreateWorkerGlobalScope(
    std::unique_ptr<GlobalScopeCreationParams> creation_params) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("audio-worklet"),
               "RealtimeAudioWorkletThread::CreateWorkerGlobalScope");
  return MakeGarbageCollected<AudioWorkletGlobalScope>(
      std::move(creation_params), this);
}

}  // namespace blink
