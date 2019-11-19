// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/audio_worklet.h"

#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/workers/worker_clients.h"
#include "third_party/blink/renderer/modules/webaudio/audio_worklet_messaging_proxy.h"
#include "third_party/blink/renderer/modules/webaudio/audio_worklet_node.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/cross_thread_audio_worklet_processor_info.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

AudioWorklet::AudioWorklet(BaseAudioContext* context)
    : Worklet(To<Document>(context->GetExecutionContext())),
      context_(context) {}

void AudioWorklet::CreateProcessor(
    scoped_refptr<AudioWorkletHandler> handler,
    MessagePortChannel message_port_channel,
    scoped_refptr<SerializedScriptValue> node_options) {
  DCHECK(IsMainThread());
  DCHECK(GetMessagingProxy());
  GetMessagingProxy()->CreateProcessor(std::move(handler),
                                       std::move(message_port_channel),
                                       std::move(node_options));
}

void AudioWorklet::NotifyGlobalScopeIsUpdated() {
  DCHECK(IsMainThread());

  if (!worklet_started_) {
    context_->NotifyWorkletIsReady();
    worklet_started_ = true;
  }
}

BaseAudioContext* AudioWorklet::GetBaseAudioContext() const {
  DCHECK(IsMainThread());
  return context_.Get();
}

const Vector<CrossThreadAudioParamInfo>
    AudioWorklet::GetParamInfoListForProcessor(
    const String& name) {
  DCHECK(IsMainThread());
  DCHECK(GetMessagingProxy());
  return GetMessagingProxy()->GetParamInfoListForProcessor(name);
}

bool AudioWorklet::IsProcessorRegistered(const String& name) {
  DCHECK(IsMainThread());
  DCHECK(GetMessagingProxy());
  return GetMessagingProxy()->IsProcessorRegistered(name);
}

bool AudioWorklet::IsReady() {
  DCHECK(IsMainThread());
  return GetMessagingProxy() && GetMessagingProxy()->GetBackingWorkerThread();
}

bool AudioWorklet::NeedsToCreateGlobalScope() {
  // This is a callback from |Worklet::FetchAndInvokeScript| call, which only
  // can be triggered by Worklet.addModule() call.
  UseCounter::Count(GetExecutionContext(), WebFeature::kAudioWorkletAddModule);

  return GetNumberOfGlobalScopes() == 0;
}

WorkletGlobalScopeProxy* AudioWorklet::CreateGlobalScope() {
  DCHECK_EQ(GetNumberOfGlobalScopes(), 0u);

  AudioWorkletMessagingProxy* proxy =
      MakeGarbageCollected<AudioWorkletMessagingProxy>(GetExecutionContext(),
                                                       this);
  proxy->Initialize(MakeGarbageCollected<WorkerClients>(),
                    ModuleResponsesMap());
  return proxy;
}

AudioWorkletMessagingProxy* AudioWorklet::GetMessagingProxy() {
  return GetNumberOfGlobalScopes() == 0
             ? nullptr
             : static_cast<AudioWorkletMessagingProxy*>(
                   FindAvailableGlobalScope());
}

void AudioWorklet::Trace(blink::Visitor* visitor) {
  visitor->Trace(context_);
  Worklet::Trace(visitor);
}

}  // namespace blink
