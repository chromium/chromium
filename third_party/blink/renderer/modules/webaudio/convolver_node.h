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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_CONVOLVER_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_CONVOLVER_NODE_H_

#include <memory>
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/threading_primitives.h"

namespace blink {

class AudioBuffer;
class ConvolverOptions;
class ExceptionState;
class Reverb;
class SharedAudioBuffer;

class MODULES_EXPORT ConvolverHandler final : public AudioHandler {
 public:
  static scoped_refptr<ConvolverHandler> Create(AudioNode&, float sample_rate);
  ~ConvolverHandler() override;

  // AudioHandler
  void Process(uint32_t frames_to_process) override;
  // Called in the main thread when the number of channels for the input may
  // have changed.
  void CheckNumberOfChannelsForInput(AudioNodeInput*) override;

  // Impulse responses
  void SetBuffer(AudioBuffer*, ExceptionState&);

  bool Normalize() const { return normalize_; }
  void SetNormalize(bool normalize) { normalize_ = normalize; }
  void SetChannelCount(unsigned, ExceptionState&) final;
  void SetChannelCountMode(const String&, ExceptionState&) final;

 private:
  ConvolverHandler(AudioNode&, float sample_rate);
  double TailTime() const override;
  double LatencyTime() const override;
  bool RequiresTailProcessing() const final;

  // Determine how many output channels to use from the number of
  // input channels and the number of channels in the impulse response
  // buffer.
  unsigned ComputeNumberOfOutputChannels(unsigned input_channels,
                                         unsigned response_channels) const;

  std::unique_ptr<Reverb> reverb_;
  std::unique_ptr<SharedAudioBuffer> shared_buffer_;

  // This synchronizes dynamic changes to the convolution impulse response with
  // process().
  mutable Mutex process_lock_;

  // Normalize the impulse response or not. Must default to true.
  bool normalize_;

  FRIEND_TEST_ALL_PREFIXES(ConvolverNodeTest, ReverbLifetime);
};

class MODULES_EXPORT ConvolverNode final : public AudioNode {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static ConvolverNode* Create(BaseAudioContext&, ExceptionState&);
  static ConvolverNode* Create(BaseAudioContext*,
                               const ConvolverOptions*,
                               ExceptionState&);

  ConvolverNode(BaseAudioContext&);

  AudioBuffer* buffer() const;
  void setBuffer(AudioBuffer*, ExceptionState&);
  bool normalize() const;
  void setNormalize(bool);

  void Trace(Visitor*) override;

  // InspectorHelperMixin
  void ReportDidCreate() final;
  void ReportWillBeDestroyed() final;

 private:
  ConvolverHandler& GetConvolverHandler() const;

  Member<AudioBuffer> buffer_;

  FRIEND_TEST_ALL_PREFIXES(ConvolverNodeTest, ReverbLifetime);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_CONVOLVER_NODE_H_
