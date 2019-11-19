// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_WORKLET_GLOBAL_SCOPE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_WORKLET_GLOBAL_SCOPE_H_

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/workers/worklet_global_scope.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/webaudio/audio_param_descriptor.h"
#include "third_party/blink/renderer/platform/audio/audio_array.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class AudioBus;
class AudioWorkletProcessor;
class AudioWorkletProcessorDefinition;
class CrossThreadAudioWorkletProcessorInfo;
class ExceptionState;
class MessagePortChannel;
class SerializedScriptValue;
class V8BlinkAudioWorkletProcessorConstructor;
struct GlobalScopeCreationParams;


// The storage for the construction of AudioWorkletProcessor, contains the
// processor name and MessageChannelPort object.
class MODULES_EXPORT ProcessorCreationParams final {
  USING_FAST_MALLOC(ProcessorCreationParams);

 public:
  ProcessorCreationParams(const String& name,
                          MessagePortChannel message_port_channel)
      : name_(name), message_port_channel_(message_port_channel) {}

  ~ProcessorCreationParams() = default;

  const String& Name() const { return name_; }
  MessagePortChannel PortChannel() {  return message_port_channel_; }

 private:
  const String name_;
  MessagePortChannel message_port_channel_;
};


// This is constructed and destroyed on a worker thread, and all methods also
// must be called on the worker thread.
class MODULES_EXPORT AudioWorkletGlobalScope final : public WorkletGlobalScope {
  DEFINE_WRAPPERTYPEINFO();

 public:
  AudioWorkletGlobalScope(std::unique_ptr<GlobalScopeCreationParams>,
                          WorkerThread*);
  ~AudioWorkletGlobalScope() override;

  bool IsAudioWorkletGlobalScope() const final { return true; }
  void Dispose() final;
  bool IsClosing() const final { return is_closing_; }

  void registerProcessor(
      const String& name,
      V8BlinkAudioWorkletProcessorConstructor* processor_ctor,
      ExceptionState&);

  // Creates an instance of AudioWorkletProcessor from a registered name.
  // This is invoked by AudioWorkletMessagingProxy upon the construction of
  // AudioWorkletNode.
  //
  // This function may return nullptr when a new V8 object cannot be constructed
  // for some reason.
  AudioWorkletProcessor* CreateProcessor(
      const String& name,
      MessagePortChannel,
      scoped_refptr<SerializedScriptValue> node_options);

  // Invokes the JS audio processing function from an instance of
  // AudioWorkletProcessor, along with given AudioBuffer from the audio graph.
  bool Process(
      AudioWorkletProcessor*,
      Vector<AudioBus*>* input_buses,
      Vector<AudioBus*>* output_buses,
      HashMap<String, std::unique_ptr<AudioFloatArray>>* param_value_map);

  AudioWorkletProcessorDefinition* FindDefinition(const String& name);

  unsigned NumberOfRegisteredDefinitions();

  std::unique_ptr<Vector<CrossThreadAudioWorkletProcessorInfo>>
      WorkletProcessorInfoListForSynchronization();

  // Gets |processor_creation_params_| for the processor construction. If there
  // is no on-going processor construction, this MUST return nullptr.
  ProcessorCreationParams* GetProcessorCreationParams();

  void SetCurrentFrame(size_t current_frame);
  void SetSampleRate(float sample_rate);

  // IDL
  uint64_t currentFrame() const { return current_frame_; }
  double currentTime() const;
  float sampleRate() const { return sample_rate_; }

  void Trace(blink::Visitor*) override;

 private:
  bool is_closing_ = false;

  typedef HeapHashMap<String, Member<AudioWorkletProcessorDefinition>>
      ProcessorDefinitionMap;
  typedef HeapVector<Member<AudioWorkletProcessor>> ProcessorInstances;

  ProcessorDefinitionMap processor_definition_map_;
  ProcessorInstances processor_instances_;

  // Gets set when the processor construction is invoked, and cleared out after
  // the construction. See the comment in |CreateProcessor()| method for the
  // detail.
  std::unique_ptr<ProcessorCreationParams> processor_creation_params_;

  size_t current_frame_ = 0;
  float sample_rate_ = 0.0;
};

template <>
struct DowncastTraits<AudioWorkletGlobalScope> {
  static bool AllowFrom(const ExecutionContext& context) {
    return context.IsAudioWorkletGlobalScope();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_WORKLET_GLOBAL_SCOPE_H_
