// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_WORKLET_OBJECT_PROXY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_WORKLET_OBJECT_PROXY_H_

#include "third_party/blink/renderer/core/workers/threaded_worklet_object_proxy.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_persistent.h"

namespace blink {

class AudioWorkletGlobalScope;
class AudioWorkletMessagingProxy;

class AudioWorkletObjectProxy final : public ThreadedWorkletObjectProxy {
 public:
  AudioWorkletObjectProxy(AudioWorkletMessagingProxy*,
                          ParentExecutionContextTaskRunners*,
                          float context_sample_rate);

  // Implements WorkerReportingProxy.
  void DidCreateWorkerGlobalScope(WorkerOrWorkletGlobalScope*) override;
  void WillDestroyWorkerGlobalScope() override;

  void SynchronizeProcessorInfoList();

 private:
  CrossThreadWeakPersistent<AudioWorkletMessagingProxy>
  GetAudioWorkletMessagingProxyWeakPtr();

  CrossThreadPersistent<AudioWorkletGlobalScope> global_scope_;

  float context_sample_rate_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_WORKLET_OBJECT_PROXY_H_
