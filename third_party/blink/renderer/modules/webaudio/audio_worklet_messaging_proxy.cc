// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/audio_worklet_messaging_proxy.h"

#include <utility>

#include "base/feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/messaging/message_port.h"
#include "third_party/blink/renderer/modules/webaudio/audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/audio_worklet.h"
#include "third_party/blink/renderer/modules/webaudio/audio_worklet_global_scope.h"
#include "third_party/blink/renderer/modules/webaudio/audio_worklet_node.h"
#include "third_party/blink/renderer/modules/webaudio/audio_worklet_object_proxy.h"
#include "third_party/blink/renderer/modules/webaudio/audio_worklet_processor.h"
#include "third_party/blink/renderer/modules/webaudio/cross_thread_audio_worklet_processor_info.h"
#include "third_party/blink/renderer/modules/webaudio/offline_audio_worklet_thread.h"
#include "third_party/blink/renderer/modules/webaudio/realtime_audio_worklet_thread.h"
#include "third_party/blink/renderer/modules/webaudio/semi_realtime_audio_worklet_thread.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_public.h"

namespace blink {

AudioWorkletMessagingProxy::AudioWorkletMessagingProxy(
    ExecutionContext* execution_context,
    AudioWorklet* worklet)
    : ThreadedWorkletMessagingProxy(execution_context), worklet_(worklet) {}

void AudioWorkletMessagingProxy::CreateProcessor(
    scoped_refptr<AudioWorkletHandler> handler,
    MessagePortChannel message_port_channel,
    scoped_refptr<SerializedScriptValue> node_options) {
  DCHECK(IsMainThread());
  PostCrossThreadTask(
      *GetWorkerThread()->GetTaskRunner(TaskType::kMiscPlatformAPI), FROM_HERE,
      CrossThreadBindOnce(
          &AudioWorkletMessagingProxy::CreateProcessorOnRenderingThread,
          WrapCrossThreadPersistent(this),
          CrossThreadUnretained(GetWorkerThread()), handler, handler->Name(),
          std::move(message_port_channel), std::move(node_options)));
}

void AudioWorkletMessagingProxy::CreateProcessorOnRenderingThread(
    WorkerThread* worker_thread,
    scoped_refptr<AudioWorkletHandler> handler,
    const String& name,
    MessagePortChannel message_port_channel,
    scoped_refptr<SerializedScriptValue> node_options) {
  DCHECK(worker_thread->IsCurrentThread());
  AudioWorkletGlobalScope* global_scope =
      To<AudioWorkletGlobalScope>(worker_thread->GlobalScope());
  AudioWorkletProcessor* processor = global_scope->CreateProcessor(
      name, message_port_channel, std::move(node_options));
  handler->SetProcessorOnRenderThread(processor);
}

void AudioWorkletMessagingProxy::SynchronizeWorkletProcessorInfoList(
    std::unique_ptr<Vector<CrossThreadAudioWorkletProcessorInfo>> info_list) {
  DCHECK(IsMainThread());
  for (auto& processor_info : *info_list) {
    processor_info_map_.insert(processor_info.Name(),
                               processor_info.ParamInfoList());
  }

  // Notify AudioWorklet object that the global scope has been updated after the
  // script evaluation.
  worklet_->NotifyGlobalScopeIsUpdated();
}

bool AudioWorkletMessagingProxy::IsProcessorRegistered(
    const String& name) const {
  return processor_info_map_.Contains(name);
}

Vector<CrossThreadAudioParamInfo>
AudioWorkletMessagingProxy::GetParamInfoListForProcessor(
    const String& name) const {
  DCHECK(IsProcessorRegistered(name));
  return processor_info_map_.at(name);
}

WorkerThread* AudioWorkletMessagingProxy::GetBackingWorkerThread() {
  return GetWorkerThread();
}

std::unique_ptr<ThreadedWorkletObjectProxy>
AudioWorkletMessagingProxy::CreateObjectProxy(
    ThreadedWorkletMessagingProxy* messaging_proxy,
    ParentExecutionContextTaskRunners* parent_execution_context_task_runners,
    scoped_refptr<base::SingleThreadTaskRunner>
        parent_agent_group_task_runner) {
  return std::make_unique<AudioWorkletObjectProxy>(
      static_cast<AudioWorkletMessagingProxy*>(messaging_proxy),
      parent_execution_context_task_runners,
      worklet_->GetBaseAudioContext()->sampleRate());
}

std::unique_ptr<WorkerThread> AudioWorkletMessagingProxy::CreateWorkerThread() {
  const auto* frame = To<LocalDOMWindow>(GetExecutionContext())->GetFrame();
  DCHECK(frame);

  std::optional<base::TimeDelta> realtime_buffer_duration;
  if (worklet_->GetBaseAudioContext()->HasRealtimeConstraint()) {
    AudioContext* context =
        static_cast<AudioContext*>(worklet_->GetBaseAudioContext());
    realtime_buffer_duration = context->PlatformBufferDuration();
  }

  return CreateWorkletThreadWithConstraints(WorkletObjectProxy(),
                                            realtime_buffer_duration,
                                            frame->IsOutermostMainFrame());
}

std::unique_ptr<WorkerThread>
AudioWorkletMessagingProxy::CreateWorkletThreadWithConstraints(
    WorkerReportingProxy& worker_reporting_proxy,
    std::optional<base::TimeDelta> realtime_buffer_duration,
    const bool is_outermost_main_frame) {
  if (!realtime_buffer_duration) {
    return std::make_unique<OfflineAudioWorkletThread>(worker_reporting_proxy);
  }

  if (is_outermost_main_frame) {
    return std::make_unique<RealtimeAudioWorkletThread>(
        worker_reporting_proxy, *realtime_buffer_duration);
  }

  return std::make_unique<SemiRealtimeAudioWorkletThread>(
      worker_reporting_proxy);
}

void AudioWorkletMessagingProxy::Trace(Visitor* visitor) const {
  visitor->Trace(worklet_);
  ThreadedWorkletMessagingProxy::Trace(visitor);
}

}  // namespace blink
