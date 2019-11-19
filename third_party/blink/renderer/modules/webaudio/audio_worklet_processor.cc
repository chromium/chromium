// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/audio_worklet_processor.h"

#include "third_party/blink/renderer/core/messaging/message_port.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/modules/webaudio/audio_buffer.h"
#include "third_party/blink/renderer/modules/webaudio/audio_worklet_global_scope.h"

namespace blink {

AudioWorkletProcessor* AudioWorkletProcessor::Create(
    ExecutionContext* context) {
  AudioWorkletGlobalScope* global_scope = To<AudioWorkletGlobalScope>(context);
  DCHECK(global_scope);
  DCHECK(global_scope->IsContextThread());

  // Get the stored initialization parameter from the global scope.
  ProcessorCreationParams* params = global_scope->GetProcessorCreationParams();
  DCHECK(params);

  auto* port = MakeGarbageCollected<MessagePort>(*global_scope);
  port->Entangle(std::move(params->PortChannel()));
  return MakeGarbageCollected<AudioWorkletProcessor>(global_scope,
                                                     params->Name(), port);
}

AudioWorkletProcessor::AudioWorkletProcessor(
    AudioWorkletGlobalScope* global_scope,
    const String& name,
    MessagePort* port)
    : global_scope_(global_scope), processor_port_(port), name_(name) {}

bool AudioWorkletProcessor::Process(
    Vector<AudioBus*>* input_buses,
    Vector<AudioBus*>* output_buses,
    HashMap<String, std::unique_ptr<AudioFloatArray>>* param_value_map) {
  DCHECK(global_scope_->IsContextThread());
  DCHECK(!hasErrorOccured());
  return global_scope_->Process(this, input_buses, output_buses,
                                param_value_map);
}

void AudioWorkletProcessor::SetErrorState(
    AudioWorkletProcessorErrorState error_state) {
  error_state_ = error_state;
}

AudioWorkletProcessorErrorState AudioWorkletProcessor::GetErrorState() const {
  return error_state_;
}

bool AudioWorkletProcessor::hasErrorOccured() const {
  return error_state_ != AudioWorkletProcessorErrorState::kNoError;
}

MessagePort* AudioWorkletProcessor::port() const {
  return processor_port_.Get();
}

void AudioWorkletProcessor::Trace(blink::Visitor* visitor) {
  visitor->Trace(global_scope_);
  visitor->Trace(processor_port_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
