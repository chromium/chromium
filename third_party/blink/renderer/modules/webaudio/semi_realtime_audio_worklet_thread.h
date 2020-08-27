// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_SEMI_REALTIME_AUDIO_WORKLET_THREAD_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_SEMI_REALTIME_AUDIO_WORKLET_THREAD_H_

#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/core/workers/worklet_thread_holder.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

class WorkerReportingProxy;
struct ThreadCreationParams;

// SemiRealtimeAudioWorkletThread is a per-AudioWorkletGlobalScope object that
// has a reference count to the backing thread that performs AudioWorklet tasks.
// This object is used by an AudioWorklet spawned by non-MainFrame and its
// backing thread has DISPLAY priority,
class MODULES_EXPORT SemiRealtimeAudioWorkletThread final : public WorkerThread {
 public:
  explicit SemiRealtimeAudioWorkletThread(WorkerReportingProxy&);
  ~SemiRealtimeAudioWorkletThread() final;

  WorkerBackingThread& GetWorkerBackingThread() final;
  void ClearWorkerBackingThread() final {}

  static void EnsureSharedBackingThread(const ThreadCreationParams&);
  static void ClearSharedBackingThread();

 private:
  WorkerOrWorkletGlobalScope* CreateWorkerGlobalScope(
      std::unique_ptr<GlobalScopeCreationParams>) final;
  bool IsOwningBackingThread() const final { return false; }
  ThreadType GetThreadType() const final {
    return ThreadType::kSemiRealtimeAudioWorkletThread;
  }

  // Use for ref-counting of all SemiRealtimeAudioWorkletThread instances in a
  // process. Incremented by the constructor and decremented by destructor.
  static int s_ref_count_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_SEMI_REALTIME_AUDIO_WORKLET_THREAD_H_
