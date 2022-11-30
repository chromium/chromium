// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/offline_audio_worklet_thread.h"

#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/modules/webaudio/audio_worklet_global_scope.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

namespace {

// Use for ref-counting of all OfflineAudioWorkletThread instances in a
// process. Incremented by the constructor and decremented by destructor.
int ref_count = 0;

void EnsureSharedBackingThread(const ThreadCreationParams& params) {
  DCHECK(IsMainThread());
  DCHECK_EQ(ref_count, 1);
  WorkletThreadHolder<OfflineAudioWorkletThread>::EnsureInstance(params);
}

}  // namespace

template class WorkletThreadHolder<OfflineAudioWorkletThread>;

OfflineAudioWorkletThread::OfflineAudioWorkletThread(
    WorkerReportingProxy& worker_reporting_proxy)
    : WorkerThread(worker_reporting_proxy) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("audio-worklet"),
               "OfflineAudioWorkletThread()");

  DCHECK(IsMainThread());

  ThreadCreationParams params =
      ThreadCreationParams(ThreadType::kOfflineAudioWorkletThread);

  // OfflineAudioWorkletThread always uses a kNormal type thread.
  params.base_thread_type = base::ThreadType::kDefault;

  if (++ref_count == 1) {
    EnsureSharedBackingThread(params);
  }
}

OfflineAudioWorkletThread::~OfflineAudioWorkletThread() {
  DCHECK(IsMainThread());
  DCHECK_GT(ref_count, 0);
  if (--ref_count == 0) {
    ClearSharedBackingThread();
  }
}

WorkerBackingThread& OfflineAudioWorkletThread::GetWorkerBackingThread() {
  return *WorkletThreadHolder<OfflineAudioWorkletThread>::GetInstance()
      ->GetThread();
}

void OfflineAudioWorkletThread::ClearSharedBackingThread() {
  DCHECK(IsMainThread());
  CHECK_EQ(ref_count, 0);
  WorkletThreadHolder<OfflineAudioWorkletThread>::ClearInstance();
}

WorkerOrWorkletGlobalScope* OfflineAudioWorkletThread::CreateWorkerGlobalScope(
    std::unique_ptr<GlobalScopeCreationParams> creation_params) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("audio-worklet"),
               "OfflineAudioWorkletThread::CreateWorkerGlobalScope");
  return MakeGarbageCollected<AudioWorkletGlobalScope>(
      std::move(creation_params), this);
}

}  // namespace blink
