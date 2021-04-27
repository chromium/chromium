// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_AUDIO_TRACK_UNDERLYING_SOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_AUDIO_TRACK_UNDERLYING_SOURCE_H_

#include "base/sequence_checker.h"
#include "media/base/audio_parameters.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_audio_sink.h"
#include "third_party/blink/renderer/modules/mediastream/frame_queue_underlying_source.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

class AudioFrameSerializationData;
class MediaStreamComponent;

class MODULES_EXPORT MediaStreamAudioTrackUnderlyingSource
    : public AudioFrameQueueUnderlyingSource,
      public WebMediaStreamAudioSink {
  USING_PRE_FINALIZER(MediaStreamAudioTrackUnderlyingSource,
                      DisconnectFromTrack);

 public:
  explicit MediaStreamAudioTrackUnderlyingSource(
      ScriptState*,
      MediaStreamComponent*,
      ScriptWrappable* media_stream_track_processor,
      wtf_size_t queue_size);
  MediaStreamAudioTrackUnderlyingSource(
      const MediaStreamAudioTrackUnderlyingSource&) = delete;
  MediaStreamAudioTrackUnderlyingSource& operator=(
      const MediaStreamAudioTrackUnderlyingSource&) = delete;

  // WebMediaStreamAudioSink
  void OnData(const media::AudioBus& audio_bus,
              base::TimeTicks estimated_capture_time) override;
  void OnSetFormat(const media::AudioParameters& params) override;

  MediaStreamComponent* Track() const { return track_.Get(); }

  void ContextDestroyed() override;
  void Trace(Visitor*) const override;
 private:
  // FrameQueueUnderlyingSource implementation.
  bool StartFrameDelivery() override;
  void StopFrameDelivery() override;

  void DisconnectFromTrack();

  void OnDataOnMainThread(std::unique_ptr<AudioFrameSerializationData> data);

  // Only used to prevent the gargabe collector from reclaiming the media
  // stream track processor that created |this|.
  const Member<ScriptWrappable> media_stream_track_processor_;

  Member<MediaStreamComponent> track_;

  media::AudioParameters audio_parameters_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_AUDIO_TRACK_UNDERLYING_SOURCE_H_
