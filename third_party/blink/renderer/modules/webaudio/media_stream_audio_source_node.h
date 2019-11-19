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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_MEDIA_STREAM_AUDIO_SOURCE_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_MEDIA_STREAM_AUDIO_SOURCE_NODE_H_

#include <memory>
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node.h"
#include "third_party/blink/renderer/platform/audio/audio_source_provider.h"
#include "third_party/blink/renderer/platform/audio/audio_source_provider_client.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"

namespace blink {

class AudioContext;
class MediaStreamAudioSourceOptions;

class MediaStreamAudioSourceHandler final : public AudioHandler {
 public:
  static scoped_refptr<MediaStreamAudioSourceHandler> Create(
      AudioNode&,
      std::unique_ptr<AudioSourceProvider>);
  ~MediaStreamAudioSourceHandler() override;

  // AudioHandler
  void Process(uint32_t frames_to_process) override;

  // AudioNode
  double TailTime() const override { return 0; }
  double LatencyTime() const override { return 0; }

  // A helper for AudioSourceProviderClient implementation of
  // MediaStreamAudioSourceNode.
  void SetFormat(uint32_t number_of_channels, float sample_rate);

  bool RequiresTailProcessing() const final { return false; }

 private:
  MediaStreamAudioSourceHandler(AudioNode&,
                                std::unique_ptr<AudioSourceProvider>);

  // As an audio source, we will never propagate silence.
  bool PropagatesSilence() const override { return false; }

  AudioSourceProvider* GetAudioSourceProvider() const {
    return audio_source_provider_.get();
  }

  std::unique_ptr<AudioSourceProvider> audio_source_provider_;

  Mutex process_lock_;

  unsigned source_number_of_channels_;
};

class MediaStreamAudioSourceNode final
    : public AudioNode,
      public AudioSourceProviderClient,
      public ActiveScriptWrappable<MediaStreamAudioSourceNode> {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(MediaStreamAudioSourceNode);

 public:
  static MediaStreamAudioSourceNode* Create(AudioContext&,
                                            MediaStream&,
                                            ExceptionState&);
  static MediaStreamAudioSourceNode*
  Create(AudioContext*, const MediaStreamAudioSourceOptions*, ExceptionState&);

  MediaStreamAudioSourceNode(AudioContext&,
                             MediaStream&,
                             MediaStreamTrack*,
                             std::unique_ptr<AudioSourceProvider>);

  void Trace(blink::Visitor*) override;

  MediaStream* getMediaStream() const;

  // AudioSourceProviderClient functions:
  void SetFormat(uint32_t number_of_channels, float sample_rate) override;

  bool HasPendingActivity() const final;

  // InspectorHelperMixin
  void ReportDidCreate() final;
  void ReportWillBeDestroyed() final;

 private:
  MediaStreamAudioSourceHandler& GetMediaStreamAudioSourceHandler() const;

  Member<MediaStreamTrack> audio_track_;
  Member<MediaStream> media_stream_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_MEDIA_STREAM_AUDIO_SOURCE_NODE_H_
