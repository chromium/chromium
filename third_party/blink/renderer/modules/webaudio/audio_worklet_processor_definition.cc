// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/audio_worklet_processor_definition.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_blink_audio_worklet_process_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_blink_audio_worklet_processor_constructor.h"
#include "third_party/blink/renderer/platform/bindings/callback_method_retriever.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

AudioWorkletProcessorDefinition* AudioWorkletProcessorDefinition::Create(
    const String& name,
    V8BlinkAudioWorkletProcessorConstructor* constructor) {
  DCHECK(!IsMainThread());
  return MakeGarbageCollected<AudioWorkletProcessorDefinition>(name,
                                                               constructor);
}

AudioWorkletProcessorDefinition::AudioWorkletProcessorDefinition(
    const String& name,
    V8BlinkAudioWorkletProcessorConstructor* constructor)
    : name_(name), constructor_(constructor) {}

AudioWorkletProcessorDefinition::~AudioWorkletProcessorDefinition() = default;

void AudioWorkletProcessorDefinition::SetAudioParamDescriptors(
    const HeapVector<Member<AudioParamDescriptor>>& descriptors) {
  audio_param_descriptors_ = descriptors;
}

const Vector<String>
    AudioWorkletProcessorDefinition::GetAudioParamDescriptorNames() const {
  Vector<String> names;
  for (const auto& descriptor : audio_param_descriptors_) {
    names.push_back(descriptor->name());
  }
  return names;
}

const AudioParamDescriptor*
    AudioWorkletProcessorDefinition::GetAudioParamDescriptor (
        const String& key) const {
  for (const auto& descriptor : audio_param_descriptors_) {
    if (descriptor->name() == key) {
      return descriptor.Get();
    }
  }
  return nullptr;
}

V8BlinkAudioWorkletProcessCallback*
AudioWorkletProcessorDefinition::ProcessFunction() {
  if (!process_function_) {
    v8::Isolate* isolate = constructor_->GetIsolate();
    ExceptionState exception_state(isolate);

    CallbackMethodRetriever retriever(constructor_);
    retriever.GetPrototypeObject(exception_state);
    if (exception_state.HadException()) {
      return nullptr;
    }

    v8::Local<v8::Value> v8_value =
        retriever.GetMethodOrUndefined("process", exception_state);
    if (exception_state.HadException() || v8_value->IsUndefined()) {
      return nullptr;
    }

    CHECK(v8_value->IsFunction());
    process_function_ =
        V8BlinkAudioWorkletProcessCallback::Create(v8_value.As<v8::Function>());
  }

  return process_function_;
}

void AudioWorkletProcessorDefinition::Trace(Visitor* visitor) const {
  visitor->Trace(constructor_);
  visitor->Trace(process_function_);
  visitor->Trace(audio_param_descriptors_);
}

}  // namespace blink
