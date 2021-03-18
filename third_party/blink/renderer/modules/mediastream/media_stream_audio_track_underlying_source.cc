// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_stream_audio_track_underlying_source.h"

#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"
#include "third_party/blink/renderer/modules/webcodecs/audio_frame_serialization_data.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_track.h"

namespace blink {

MediaStreamAudioTrackUnderlyingSource::MediaStreamAudioTrackUnderlyingSource(
    ScriptState* script_state,
    MediaStreamComponent* track,
    wtf_size_t max_queue_size)
    : AudioFrameQueueUnderlyingSource(script_state, max_queue_size),
      track_(track) {
  DCHECK(track_);
}

bool MediaStreamAudioTrackUnderlyingSource::StartFrameDelivery() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MediaStreamAudioTrack* audio_track = MediaStreamAudioTrack::From(track_);
  if (!audio_track)
    return false;

  WebMediaStreamAudioSink::AddToAudioTrack(this, WebMediaStreamTrack(track_));
  return true;
}

void MediaStreamAudioTrackUnderlyingSource::DisconnectFromTrack() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!track_)
    return;

  WebMediaStreamAudioSink::RemoveFromAudioTrack(this,
                                                WebMediaStreamTrack(track_));

  track_.Clear();
}

void MediaStreamAudioTrackUnderlyingSource::Trace(Visitor* visitor) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  visitor->Trace(track_);
  AudioFrameQueueUnderlyingSource::Trace(visitor);
}

void MediaStreamAudioTrackUnderlyingSource::OnData(
    const media::AudioBus& audio_bus,
    base::TimeTicks estimated_capture_time) {
  DCHECK(audio_parameters_.IsValid());

  auto data_copy =
      media::AudioBus::Create(audio_bus.channels(), audio_bus.frames());
  audio_bus.CopyTo(data_copy.get());

  auto queue_data = AudioFrameSerializationData::Wrap(
      std::move(data_copy), audio_parameters_.sample_rate(),
      estimated_capture_time - base::TimeTicks());

  QueueFrame(std::move(queue_data));
}

void MediaStreamAudioTrackUnderlyingSource::StopFrameDelivery() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DisconnectFromTrack();
}

void MediaStreamAudioTrackUnderlyingSource::OnSetFormat(
    const media::AudioParameters& params) {
  DCHECK(params.IsValid());
  audio_parameters_ = params;
}

}  // namespace blink
