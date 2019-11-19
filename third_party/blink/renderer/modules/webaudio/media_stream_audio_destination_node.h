/*
 * Copyright (C) 2012, Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_MEDIA_STREAM_AUDIO_DESTINATION_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_MEDIA_STREAM_AUDIO_DESTINATION_NODE_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream.h"
#include "third_party/blink/renderer/modules/webaudio/audio_basic_inspector_node.h"
#include "third_party/blink/renderer/platform/audio/audio_bus.h"

namespace blink {

class AudioContext;

class MediaStreamAudioDestinationHandler final
    : public AudioBasicInspectorHandler {
 public:
  static scoped_refptr<MediaStreamAudioDestinationHandler> Create(
      AudioNode&,
      uint32_t number_of_channels);
  ~MediaStreamAudioDestinationHandler() override;

  // AudioHandler.
  void Process(uint32_t frames_to_process) override;
  void SetChannelCount(unsigned, ExceptionState&) override;

  uint32_t MaxChannelCount() const;

  bool RequiresTailProcessing() const final { return false; }

  // This node has no outputs, so we need methods that are different from the
  // ones provided by AudioBasicInspectorHnadler, which assume an output.
  void PullInputs(uint32_t frames_to_process) override;
  void CheckNumberOfChannelsForInput(AudioNodeInput*) override;

  // AudioNode
  void UpdatePullStatusIfNeeded() override;

 private:
  MediaStreamAudioDestinationHandler(AudioNode&, uint32_t number_of_channels);
  // As an audio source, we will never propagate silence.
  bool PropagatesSilence() const override { return false; }

  // MediaStreamSource is held alive by MediaStreamAudioDestinationNode.
  // Accessed by main thread and during audio thread processing.
  //
  // TODO: try to avoid such access during audio thread processing.
  CrossThreadWeakPersistent<MediaStreamSource> source_;

  // This synchronizes dynamic changes to the channel count with
  // process() to manage the mix bus.
  mutable Mutex process_lock_;

  // This internal mix bus is for up/down mixing the input to the actual
  // number of channels in the destination.
  scoped_refptr<AudioBus> mix_bus_;
};

class MediaStreamAudioDestinationNode final : public AudioBasicInspectorNode {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static MediaStreamAudioDestinationNode* Create(AudioContext&,
                                                 uint32_t number_of_channels,
                                                 ExceptionState&);
  static MediaStreamAudioDestinationNode* Create(AudioContext*,
                                                 const AudioNodeOptions*,
                                                 ExceptionState&);

  MediaStreamAudioDestinationNode(AudioContext&, uint32_t number_of_channels);

  MediaStream* stream() const { return stream_; }
  MediaStreamSource* source() const { return source_; }

  void Trace(Visitor*) final;

  // InspectorHelperMixin
  void ReportDidCreate() final;
  void ReportWillBeDestroyed() final;

 private:
  const Member<MediaStreamSource> source_;
  const Member<MediaStream> stream_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_MEDIA_STREAM_AUDIO_DESTINATION_NODE_H_
