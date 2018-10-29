// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/audio_worklet_thread.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/worker_backing_thread.h"
#include "third_party/blink/renderer/modules/webaudio/audio_worklet.h"
#include "third_party/blink/renderer/modules/webaudio/audio_worklet_global_scope.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/waitable_event.h"
#include "third_party/blink/renderer/platform/web_thread_supporting_gc.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

template class WorkletThreadHolder<AudioWorkletThread>;

unsigned AudioWorkletThread::s_ref_count_ = 0;

std::unique_ptr<AudioWorkletThread> AudioWorkletThread::Create(
    WorkerReportingProxy& worker_reporting_proxy) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("audio-worklet"),
               "AudioWorkletThread::create");
  return base::WrapUnique(new AudioWorkletThread(worker_reporting_proxy));
}

AudioWorkletThread::AudioWorkletThread(
    WorkerReportingProxy& worker_reporting_proxy)
    : WorkerThread(worker_reporting_proxy) {
  DCHECK(IsMainThread());
  if (++s_ref_count_ == 1) {
    EnsureSharedBackingThread();
  }
}

AudioWorkletThread::~AudioWorkletThread() {
  DCHECK(IsMainThread());
  if (--s_ref_count_ == 0) {
    ClearSharedBackingThread();
  }
}

WorkerBackingThread& AudioWorkletThread::GetWorkerBackingThread() {
  return *WorkletThreadHolder<AudioWorkletThread>::GetInstance()->GetThread();
}

void AudioWorkletThread::EnsureSharedBackingThread() {
  DCHECK(IsMainThread());
  WorkletThreadHolder<AudioWorkletThread>::EnsureInstance(
      ThreadCreationParams(WebThreadType::kWebAudioThread));
}

void AudioWorkletThread::ClearSharedBackingThread() {
  DCHECK(IsMainThread());
  DCHECK_EQ(s_ref_count_, 0u);
  WorkletThreadHolder<AudioWorkletThread>::ClearInstance();
}

WorkerOrWorkletGlobalScope* AudioWorkletThread::CreateWorkerGlobalScope(
    std::unique_ptr<GlobalScopeCreationParams> creation_params) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("audio-worklet"),
               "AudioWorkletThread::createWorkerGlobalScope");
  return AudioWorkletGlobalScope::Create(std::move(creation_params), this);
}

}  // namespace blink
