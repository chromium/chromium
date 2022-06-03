// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_WORKLET_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_WORKLET_NODE_H_

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_worklet_node_options.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node.h"
#include "third_party/blink/renderer/modules/webaudio/audio_param_map.h"
#include "third_party/blink/renderer/modules/webaudio/audio_worklet_processor_error_state.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"

namespace blink {

class AudioNodeInput;
class AudioWorkletProcessor;
class BaseAudioContext;
class CrossThreadAudioParamInfo;
class ExceptionState;
class MessagePort;
class ScriptState;

// AudioWorkletNode is a user-facing interface of custom audio processor in
// Web Audio API. The integration of WebAudio renderer is done via
// AudioWorkletHandler and the actual audio processing runs on
// AudioWorkletProcess.
//
//               [Main Scope]                   |    [AudioWorkletGlobalScope]
//  AudioWorkletNode <-> AudioWorkletHandler <==|==>   AudioWorkletProcessor
//   (JS interface)       (Renderer access)     |      (V8 audio processing)

class AudioWorkletHandler final
    : public AudioHandler,
      public base::SupportsWeakPtr<AudioWorkletHandler> {
 public:
  static scoped_refptr<AudioWorkletHandler> Create(
      AudioNode&,
      float sample_rate,
      String name,
      HashMap<String, scoped_refptr<AudioParamHandler>> param_handler_map,
      const AudioWorkletNodeOptions*);

  ~AudioWorkletHandler() override;

  // Called from render thread.
  void Process(uint32_t frames_to_process) override;

  void CheckNumberOfChannelsForInput(AudioNodeInput*) override;
  void UpdatePullStatusIfNeeded() override;

  double TailTime() const override;
  double LatencyTime() const override { return 0; }

  String Name() const { return name_; }

  // Sets |AudioWorkletProcessor| and changes the state of the processor.
  // MUST be called from the render thread.
  void SetProcessorOnRenderThread(AudioWorkletProcessor*);

  // Finish |AudioWorkletProcessor| and set the tail time to zero, when
  // the user-supplied |process()| method returns false.
  void FinishProcessorOnRenderThread();

  void NotifyProcessorError(AudioWorkletProcessorErrorState);

 private:
  AudioWorkletHandler(
      AudioNode&,
      float sample_rate,
      String name,
      HashMap<String, scoped_refptr<AudioParamHandler>> param_handler_map,
      const AudioWorkletNodeOptions*);

  String name_;

  double tail_time_ = std::numeric_limits<double>::infinity();

  // MUST be set/used by render thread.
  CrossThreadPersistent<AudioWorkletProcessor> processor_;

  // Keeps the reference of AudioBus objects from AudioNodeInput and
  // AudioNodeOutput in order to pass them to AudioWorkletProcessor.
  Vector<scoped_refptr<AudioBus>> inputs_;
  Vector<scoped_refptr<AudioBus>> outputs_;

  HashMap<String, scoped_refptr<AudioParamHandler>> param_handler_map_;
  HashMap<String, std::unique_ptr<AudioFloatArray>> param_value_map_;

  // TODO(): Adjust this if needed based on the result of the process
  // method or the value of |tail_time_|.
  bool RequiresTailProcessing() const override { return true; }

  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  // Used only if number of inputs and outputs are 1.
  bool is_output_channel_count_given_ = false;
};

class AudioWorkletNode final : public AudioNode,
                               public ActiveScriptWrappable<AudioWorkletNode> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static AudioWorkletNode* Create(ScriptState*,
                                  BaseAudioContext*,
                                  const String& name,
                                  const AudioWorkletNodeOptions*,
                                  ExceptionState&);

  AudioWorkletNode(BaseAudioContext&,
                   const String& name,
                   const AudioWorkletNodeOptions*,
                   const Vector<CrossThreadAudioParamInfo>,
                   MessagePort* node_port);

  // ActiveScriptWrappable
  bool HasPendingActivity() const final;

  // IDL
  AudioParamMap* parameters() const;
  MessagePort* port() const;
  DEFINE_ATTRIBUTE_EVENT_LISTENER(processorerror, kError)

  void FireProcessorError(AudioWorkletProcessorErrorState);

  void Trace(Visitor*) const override;

  // InspectorHelperMixin
  void ReportDidCreate() final;
  void ReportWillBeDestroyed() final;

 private:
  scoped_refptr<AudioWorkletHandler> GetWorkletHandler() const;

  Member<AudioParamMap> parameter_map_;
  Member<MessagePort> node_port_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_WORKLET_NODE_H_
