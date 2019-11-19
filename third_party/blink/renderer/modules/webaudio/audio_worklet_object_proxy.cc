// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/audio_worklet_object_proxy.h"

#include "third_party/blink/renderer/core/workers/threaded_worklet_messaging_proxy.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/modules/webaudio/audio_worklet_global_scope.h"
#include "third_party/blink/renderer/modules/webaudio/audio_worklet_messaging_proxy.h"
#include "third_party/blink/renderer/modules/webaudio/cross_thread_audio_worklet_processor_info.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

AudioWorkletObjectProxy::AudioWorkletObjectProxy(
    AudioWorkletMessagingProxy* messaging_proxy_weak_ptr,
    ParentExecutionContextTaskRunners* parent_execution_context_task_runners,
    float context_sample_rate)
    : ThreadedWorkletObjectProxy(
          static_cast<ThreadedWorkletMessagingProxy*>(messaging_proxy_weak_ptr),
          parent_execution_context_task_runners),
      context_sample_rate_(context_sample_rate) {}

void AudioWorkletObjectProxy::DidCreateWorkerGlobalScope(
    WorkerOrWorkletGlobalScope* global_scope) {
  global_scope_ = To<AudioWorkletGlobalScope>(global_scope);
  global_scope_->SetSampleRate(context_sample_rate_);
}

void AudioWorkletObjectProxy::DidEvaluateModuleScript(bool success) {
  DCHECK(global_scope_);

  if (!success || global_scope_->NumberOfRegisteredDefinitions() == 0)
    return;

  std::unique_ptr<Vector<CrossThreadAudioWorkletProcessorInfo>>
      processor_info_list =
          global_scope_->WorkletProcessorInfoListForSynchronization();

  if (processor_info_list->size() == 0)
    return;

  // This method is called by a loading task which calls
  // WorkletModuleTreeClient::NotifyModuleTreeLoadFinished and
  // SynchronizeWorkletProcessorInfoList needs to run in FIFO order with other
  // loading tasks.
  PostCrossThreadTask(
      *GetParentExecutionContextTaskRunners()->Get(TaskType::kInternalLoading),
      FROM_HERE,
      CrossThreadBindOnce(
          &AudioWorkletMessagingProxy::SynchronizeWorkletProcessorInfoList,
          GetAudioWorkletMessagingProxyWeakPtr(),
          WTF::Passed(std::move(processor_info_list))));
}

void AudioWorkletObjectProxy::WillDestroyWorkerGlobalScope() {
  global_scope_ = nullptr;
}

CrossThreadWeakPersistent<AudioWorkletMessagingProxy>
AudioWorkletObjectProxy::GetAudioWorkletMessagingProxyWeakPtr() {
  return WrapCrossThreadWeakPersistent(
      static_cast<AudioWorkletMessagingProxy*>(MessagingProxyWeakPtr().Get()));
}

}  // namespace blink
