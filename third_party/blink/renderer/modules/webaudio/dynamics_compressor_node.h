/*
 * Copyright (C) 2011, Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_DYNAMICS_COMPRESSOR_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_DYNAMICS_COMPRESSOR_NODE_H_

#include <atomic>
#include <memory>
#include "base/gtest_prod_util.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node.h"
#include "third_party/blink/renderer/modules/webaudio/audio_param.h"

namespace blink {

class BaseAudioContext;
class DynamicsCompressor;
class DynamicsCompressorOptions;

class MODULES_EXPORT DynamicsCompressorHandler final : public AudioHandler {
 public:
  static scoped_refptr<DynamicsCompressorHandler> Create(
      AudioNode&,
      float sample_rate,
      AudioParamHandler& threshold,
      AudioParamHandler& knee,
      AudioParamHandler& ratio,
      AudioParamHandler& attack,
      AudioParamHandler& release);

  ~DynamicsCompressorHandler() override;

  // AudioHandler
  void Process(uint32_t frames_to_process) override;
  void ProcessOnlyAudioParams(uint32_t frames_to_process) override;
  void Initialize() override;

  float ReductionValue() const {
    return reduction_.load(std::memory_order_relaxed);
  }

  void SetChannelCount(unsigned, ExceptionState&) final;
  void SetChannelCountMode(const String&, ExceptionState&) final;

 private:
  DynamicsCompressorHandler(AudioNode&,
                            float sample_rate,
                            AudioParamHandler& threshold,
                            AudioParamHandler& knee,
                            AudioParamHandler& ratio,
                            AudioParamHandler& attack,
                            AudioParamHandler& release);
  bool RequiresTailProcessing() const final;
  double TailTime() const override;
  double LatencyTime() const override;

  std::unique_ptr<DynamicsCompressor> dynamics_compressor_;
  scoped_refptr<AudioParamHandler> threshold_;
  scoped_refptr<AudioParamHandler> knee_;
  scoped_refptr<AudioParamHandler> ratio_;
  std::atomic<float> reduction_;
  scoped_refptr<AudioParamHandler> attack_;
  scoped_refptr<AudioParamHandler> release_;

  FRIEND_TEST_ALL_PREFIXES(DynamicsCompressorNodeTest, ProcessorLifetime);
};

class MODULES_EXPORT DynamicsCompressorNode final : public AudioNode {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static DynamicsCompressorNode* Create(BaseAudioContext&, ExceptionState&);
  static DynamicsCompressorNode* Create(BaseAudioContext*,
                                        const DynamicsCompressorOptions*,
                                        ExceptionState&);

  DynamicsCompressorNode(BaseAudioContext&);

  void Trace(blink::Visitor*) override;

  AudioParam* threshold() const;
  AudioParam* knee() const;
  AudioParam* ratio() const;
  float reduction() const;
  AudioParam* attack() const;
  AudioParam* release() const;

  // InspectorHelperMixin
  void ReportDidCreate() final;
  void ReportWillBeDestroyed() final;

 private:
  DynamicsCompressorHandler& GetDynamicsCompressorHandler() const;

  Member<AudioParam> threshold_;
  Member<AudioParam> knee_;
  Member<AudioParam> ratio_;
  Member<AudioParam> attack_;
  Member<AudioParam> release_;

  FRIEND_TEST_ALL_PREFIXES(DynamicsCompressorNodeTest, ProcessorLifetime);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_DYNAMICS_COMPRESSOR_NODE_H_
