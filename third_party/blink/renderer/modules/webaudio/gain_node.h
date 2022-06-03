/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_GAIN_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_GAIN_NODE_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node.h"
#include "third_party/blink/renderer/modules/webaudio/audio_param.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"

namespace blink {

class BaseAudioContext;
class GainOptions;

// GainNode is an AudioNode with one input and one output which applies a gain
// (volume) change to the audio signal.  De-zippering (smoothing) is applied
// when the gain value is changed dynamically.

class GainHandler final : public AudioHandler {
 public:
  static scoped_refptr<GainHandler> Create(AudioNode&,
                                           float sample_rate,
                                           AudioParamHandler& gain);

  // AudioHandler
  void Process(uint32_t frames_to_process) override;
  void ProcessOnlyAudioParams(uint32_t frames_to_process) override;

  // Called in the main thread when the number of channels for the input may
  // have changed.
  void CheckNumberOfChannelsForInput(AudioNodeInput*) override;

  // AudioNode
  double TailTime() const override { return 0; }
  double LatencyTime() const override { return 0; }
  bool RequiresTailProcessing() const final { return false; }

 private:
  GainHandler(AudioNode&, float sample_rate, AudioParamHandler& gain);

  scoped_refptr<AudioParamHandler> gain_;

  AudioFloatArray sample_accurate_gain_values_;
};

class GainNode final : public AudioNode {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static GainNode* Create(BaseAudioContext&, ExceptionState&);
  static GainNode* Create(BaseAudioContext*,
                          const GainOptions*,
                          ExceptionState&);

  GainNode(BaseAudioContext&);

  void Trace(Visitor*) const override;

  AudioParam* gain() const;

  // InspectorHelperMixin
  void ReportDidCreate() final;
  void ReportWillBeDestroyed() final;

 private:
  Member<AudioParam> gain_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_GAIN_NODE_H_
