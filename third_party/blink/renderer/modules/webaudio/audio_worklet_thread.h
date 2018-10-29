// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_WORKLET_THREAD_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_WORKLET_THREAD_H_

#include <memory>
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/core/workers/worklet_thread_holder.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

class WorkerReportingProxy;

// AudioWorkletThread is a per-frame singleton object that has a reference count
// to the backing thread for the processing of AudioWorkletNode and
// AudioWorkletProcessor.

class MODULES_EXPORT AudioWorkletThread final : public WorkerThread {
 public:
  static std::unique_ptr<AudioWorkletThread> Create(WorkerReportingProxy&);
  ~AudioWorkletThread() override;

  WorkerBackingThread& GetWorkerBackingThread() override;

  // The backing thread is cleared by clearSharedBackingThread().
  void ClearWorkerBackingThread() override {}

  static void EnsureSharedBackingThread();
  static void ClearSharedBackingThread();

 private:
  explicit AudioWorkletThread(WorkerReportingProxy&);

  WorkerOrWorkletGlobalScope* CreateWorkerGlobalScope(
      std::unique_ptr<GlobalScopeCreationParams>) final;

  bool IsOwningBackingThread() const override { return false; }

  WebThreadType GetThreadType() const override {
    return WebThreadType::kAudioWorkletThread;
  }

  // This is only accessed by the main thread. Incremented by the constructor,
  // and decremented by destructor.
  static unsigned s_ref_count_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_WORKLET_THREAD_H_
