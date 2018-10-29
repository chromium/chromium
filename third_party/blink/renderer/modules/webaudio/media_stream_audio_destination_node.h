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
      size_t number_of_channels);
  ~MediaStreamAudioDestinationHandler() override;

  MediaStream* Stream() { return stream_.Get(); }

  // AudioHandler.
  void Process(size_t frames_to_process) override;
  void SetChannelCount(unsigned long, ExceptionState&) override;

  unsigned long MaxChannelCount() const;

  bool RequiresTailProcessing() const final { return false; }

 private:
  MediaStreamAudioDestinationHandler(AudioNode&, size_t number_of_channels);
  // As an audio source, we will never propagate silence.
  bool PropagatesSilence() const override { return false; }

  // This Persistent doesn't make a reference cycle.
  Persistent<MediaStream> stream_;
  // Accessed by main thread and during audio thread processing.
  //
  // TODO: try to avoid such access during audio thread processing.
  CrossThreadPersistent<MediaStreamSource> source_;

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
                                                 size_t number_of_channels,
                                                 ExceptionState&);
  static MediaStreamAudioDestinationNode* Create(AudioContext*,
                                                 const AudioNodeOptions&,
                                                 ExceptionState&);

  MediaStream* stream() const;

 private:
  MediaStreamAudioDestinationNode(AudioContext&, size_t number_of_channels);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_MEDIA_STREAM_AUDIO_DESTINATION_NODE_H_
