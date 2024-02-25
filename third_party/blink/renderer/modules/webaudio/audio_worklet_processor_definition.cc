// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/audio_worklet_processor_definition.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_blink_audio_worklet_process_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_blink_audio_worklet_processor_constructor.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

AudioWorkletProcessorDefinition* AudioWorkletProcessorDefinition::Create(
    const String& name,
    V8BlinkAudioWorkletProcessorConstructor* constructor,
    V8BlinkAudioWorkletProcessCallback* process) {
  DCHECK(!IsMainThread());
  return MakeGarbageCollected<AudioWorkletProcessorDefinition>(
      name, constructor, process);
}

AudioWorkletProcessorDefinition::AudioWorkletProcessorDefinition(
    const String& name,
    V8BlinkAudioWorkletProcessorConstructor* constructor,
    V8BlinkAudioWorkletProcessCallback* process)
    : name_(name), constructor_(constructor), process_(process) {}

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

void AudioWorkletProcessorDefinition::Trace(Visitor* visitor) const {
  visitor->Trace(constructor_);
  visitor->Trace(process_);
  visitor->Trace(audio_param_descriptors_);
}

}  // namespace blink
