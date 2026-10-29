// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_WORKLET_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_WORKLET_NODE_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_worklet_node_options.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node.h"
#include "third_party/blink/renderer/modules/webaudio/audio_param_map.h"
#include "third_party/blink/renderer/modules/webaudio/audio_worklet_handler.h"
#include "third_party/blink/renderer/modules/webaudio/audio_worklet_processor_error_details.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"

namespace blink {

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
class MODULES_EXPORT AudioWorkletNode final
    : public AudioNode,
      public ActiveScriptWrappable<AudioWorkletNode> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static AudioWorkletNode* Create(ScriptState* script_state,
                                  BaseAudioContext* context,
                                  const String& name,
                                  const AudioWorkletNodeOptions* options,
                                  ExceptionState& exception_state);

  AudioWorkletNode(BaseAudioContext& context,
                   const String& name,
                   const AudioWorkletNodeOptions* options,
                   const Vector<CrossThreadAudioParamInfo> param_info_list,
                   MessagePort* node_port);

  // ActiveScriptWrappable
  bool HasPendingActivity() const final;

  // IDL
  AudioParamMap* parameters() const;
  MessagePort* port() const;
  EventListener* onprocessorerror();
  void setOnprocessorerror(EventListener* listener);

  void FireProcessorError(
      const AudioWorkletProcessorErrorDetails& error_details);

  void Trace(Visitor* visitor) const override;

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
