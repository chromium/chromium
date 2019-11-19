// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_CONSTANT_SOURCE_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_CONSTANT_SOURCE_NODE_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/modules/webaudio/audio_param.h"
#include "third_party/blink/renderer/modules/webaudio/audio_scheduled_source_node.h"
#include "third_party/blink/renderer/platform/audio/audio_bus.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"

namespace blink {

class BaseAudioContext;
class ConstantSourceOptions;
class ExceptionState;

// ConstantSourceNode is an audio generator for a constant source

class ConstantSourceHandler final : public AudioScheduledSourceHandler {
 public:
  static scoped_refptr<ConstantSourceHandler> Create(AudioNode&,
                                                     float sample_rate,
                                                     AudioParamHandler& offset);
  ~ConstantSourceHandler() override;

  // AudioHandler
  void Process(uint32_t frames_to_process) override;

  void HandleStoppableSourceNode() override;

 private:
  ConstantSourceHandler(AudioNode&,
                        float sample_rate,
                        AudioParamHandler& offset);

  // If we are no longer playing, propogate silence ahead to downstream nodes.
  bool PropagatesSilence() const override;

  scoped_refptr<AudioParamHandler> offset_;
  AudioFloatArray sample_accurate_values_;
};

class ConstantSourceNode final : public AudioScheduledSourceNode {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static ConstantSourceNode* Create(BaseAudioContext&, ExceptionState&);
  static ConstantSourceNode* Create(BaseAudioContext*,
                                    const ConstantSourceOptions*,
                                    ExceptionState&);

  ConstantSourceNode(BaseAudioContext&);
  void Trace(blink::Visitor*) override;

  AudioParam* offset();

  ConstantSourceHandler& GetConstantSourceHandler() const;

  // InspectorHelperMixin
  void ReportDidCreate() final;
  void ReportWillBeDestroyed() final;

 private:
  Member<AudioParam> offset_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_CONSTANT_SOURCE_NODE_H_
