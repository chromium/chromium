// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/realtime_audio_worklet_thread.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/modules/webaudio/audio_worklet_global_scope.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

namespace {

// The realtime AudioWorklet thread is managed by a pool system. The system
// can contain up to 4 concurrent real-time threads and it is based on “first
// come first served” policy.
// - The 1st ~ 3rd threads are a “dedicated” thread. The first 3 AudioWorklets
//   will have their own dedicated backing thread.
// - The 4th thread is a “shared” thread: Starting from the 4th AudioWorklet,
//   all subsequent contexts will share the same thread for the AudioWorklet
//   operation.
static constexpr int kMaxDedicatedBackingThreadCount = 3;

// Used for counting dedicated backing threads. Incremented by the constructor
// and decremented by destructor.
int dedicated_backing_thread_count = 0;

// Used for ref-counting of all backing thread in the current renderer process.
// Incremented by the constructor and decremented by destructor.
int shared_backing_thread_ref_count = 0;

// For UMA logging: Represents the maximum number of dedicated backing worklet
// threads throughout the lifetime of the document/frame. Can't exceed
// `kMaxDedicatedBackingThreadCount`.
int peak_dedicated_backing_thread_count = 0;

// For UMA logging: Represents the maximum number of ref counts using the
// shared backing thread throughout the lifetime of the document/frame.
int peak_shared_backing_thread_ref_count = 0;

}  // namespace

template class WorkletThreadHolder<RealtimeAudioWorkletThread>;

RealtimeAudioWorkletThread::RealtimeAudioWorkletThread(
    WorkerReportingProxy& worker_reporting_proxy,
    base::TimeDelta realtime_buffer_duration)
    : WorkerThread(worker_reporting_proxy) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("audio-worklet"),
               "RealtimeAudioWorkletThread()");

  DCHECK(IsMainThread());

  ThreadCreationParams params =
      ThreadCreationParams(ThreadType::kRealtimeAudioWorkletThread);

  // The real-time priority thread is enabled by default. A normal priority
  // thread is used when it is blocked by a field trial.
  if (base::FeatureList::IsEnabled(
          features::kAudioWorkletThreadRealtimePriority)) {
    // TODO(crbug.com/1022888): The worklet thread priority is always NORMAL on
    // Linux and Chrome OS regardless of this thread priority setting.
    params.base_thread_type = base::ThreadType::kRealtimeAudio;
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("audio-worklet"),
                 "RealtimeAudioWorkletThread() - kRealtimeAudio");
#if BUILDFLAG(IS_APPLE)
    if (base::FeatureList::IsEnabled(
            features::kAudioWorkletThreadRealtimePeriodMac)) {
      params.realtime_period = realtime_buffer_duration;
      TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("audio-worklet"),
                   "RealtimeAudioWorkletThread()", "realtime period",
                   realtime_buffer_duration);
    }
#endif
  } else {
    params.base_thread_type = base::ThreadType::kDefault;
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("audio-worklet"),
                 "RealtimeAudioWorkletThread() - kDefault");
  }

  if (base::FeatureList::IsEnabled(features::kAudioWorkletThreadPool) &&
      dedicated_backing_thread_count < kMaxDedicatedBackingThreadCount) {
    worker_backing_thread_ = std::make_unique<WorkerBackingThread>(params);
    dedicated_backing_thread_count++;
    if (peak_dedicated_backing_thread_count < dedicated_backing_thread_count) {
      peak_dedicated_backing_thread_count = dedicated_backing_thread_count;
      base::UmaHistogramExactLinear(
          "WebAudio.AudioWorklet.PeakDedicatedBackingThreadCount",
          peak_dedicated_backing_thread_count,
          kMaxDedicatedBackingThreadCount + 1);
    }
  } else {
    if (!shared_backing_thread_ref_count) {
      WorkletThreadHolder<RealtimeAudioWorkletThread>::EnsureInstance(params);
    }
    shared_backing_thread_ref_count++;
    if (peak_shared_backing_thread_ref_count <
        shared_backing_thread_ref_count) {
      peak_shared_backing_thread_ref_count = shared_backing_thread_ref_count;
      base::UmaHistogramExactLinear(
          "WebAudio.AudioWorklet.PeakSharedBackingThreadRefCount",
          peak_shared_backing_thread_ref_count, 101);
    }
  }
}

RealtimeAudioWorkletThread::~RealtimeAudioWorkletThread() {
  DCHECK(IsMainThread());

  if (worker_backing_thread_) {
    dedicated_backing_thread_count--;
    CHECK_GE(dedicated_backing_thread_count, 0);
  } else {
    shared_backing_thread_ref_count--;
    CHECK_GE(shared_backing_thread_ref_count, 0);
    if (!shared_backing_thread_ref_count) {
      WorkletThreadHolder<RealtimeAudioWorkletThread>::ClearInstance();
    }
  }
}

WorkerBackingThread& RealtimeAudioWorkletThread::GetWorkerBackingThread() {
  if (worker_backing_thread_) {
    return *worker_backing_thread_.get();
  }

  auto* shared_thread_instance =
      WorkletThreadHolder<RealtimeAudioWorkletThread>::GetInstance();
  CHECK(shared_thread_instance);
  return *shared_thread_instance->GetThread();
}

WorkerOrWorkletGlobalScope* RealtimeAudioWorkletThread::CreateWorkerGlobalScope(
    std::unique_ptr<GlobalScopeCreationParams> creation_params) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("audio-worklet"),
               "RealtimeAudioWorkletThread::CreateWorkerGlobalScope");
  return MakeGarbageCollected<AudioWorkletGlobalScope>(
      std::move(creation_params), this);
}

}  // namespace blink
