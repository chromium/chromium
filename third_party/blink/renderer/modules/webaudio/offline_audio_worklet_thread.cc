// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/offline_audio_worklet_thread.h"

#include "third_party/blink/renderer/modules/webaudio/audio_worklet_global_scope.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

template class WorkletThreadHolder<OfflineAudioWorkletThread>;

int OfflineAudioWorkletThread::s_ref_count_ = 0;

OfflineAudioWorkletThread::OfflineAudioWorkletThread(
    WorkerReportingProxy& worker_reporting_proxy)
    : WorkerThread(worker_reporting_proxy) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("audio-worklet"),
               "OfflineAudioWorkletThread()");

  DCHECK(IsMainThread());

  ThreadCreationParams params =
      ThreadCreationParams(ThreadType::kOfflineAudioWorkletThread);

  // OfflineAudioWorkletThread always uses a NORMAL priority thread.
  params.thread_priority = base::ThreadPriority::NORMAL;

  if (++s_ref_count_ == 1)
    EnsureSharedBackingThread(params);
}

OfflineAudioWorkletThread::~OfflineAudioWorkletThread() {
  DCHECK(IsMainThread());
  if (--s_ref_count_ == 0)
    ClearSharedBackingThread();
}

WorkerBackingThread& OfflineAudioWorkletThread::GetWorkerBackingThread() {
  return *WorkletThreadHolder<OfflineAudioWorkletThread>::GetInstance()
      ->GetThread();
}

void OfflineAudioWorkletThread::EnsureSharedBackingThread(
    const ThreadCreationParams& params) {
  DCHECK(IsMainThread());
  WorkletThreadHolder<OfflineAudioWorkletThread>::EnsureInstance(params);
}

void OfflineAudioWorkletThread::ClearSharedBackingThread() {
  DCHECK(IsMainThread());
  CHECK_EQ(s_ref_count_, 0);
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
