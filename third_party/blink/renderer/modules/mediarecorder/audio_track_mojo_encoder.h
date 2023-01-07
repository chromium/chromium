// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIARECORDER_AUDIO_TRACK_MOJO_ENCODER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIARECORDER_AUDIO_TRACK_MOJO_ENCODER_H_

#include <memory>

#include "base/containers/queue.h"
#include "base/memory/weak_ptr.h"
#include "media/base/audio_encoder.h"
#include "media/base/encoder_status.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/modules/mediarecorder/audio_track_encoder.h"
#include "third_party/blink/renderer/modules/mediarecorder/audio_track_recorder.h"

namespace base {
class TimeTicks;
}

namespace media {
class AudioBus;
class AudioParameters;
class MojoAudioEncoder;
}  // namespace media

namespace blink {

// A thin wrapper for platform audio encoders which run in the GPU process.
// Currently, the only available encoder implementation is AAC, and only on
// Windows and Mac.
//
// This class uses a MojoAudioEncoder, which requires us to be async, so input
// may be buffered in this class, and will be asynchronously delivered via
// `on_encoded_audio_cb_`.
//
// Some encoders may buffer input frames, and MediaRecorder's abrupt stop design
// does not allow us to Flush. So, we may never receive the output for them,
// losing some audio at the end of the recording.
class AudioTrackMojoEncoder : public AudioTrackEncoder {
 public:
  AudioTrackMojoEncoder(AudioTrackRecorder::CodecId codec,
                        OnEncodedAudioCB on_encoded_audio_cb,
                        uint32_t bits_per_second = 0);

  AudioTrackMojoEncoder(const AudioTrackMojoEncoder&) = delete;
  AudioTrackMojoEncoder& operator=(const AudioTrackMojoEncoder&) = delete;

  // Destroys the existing `mojo_encoder_` and recreates it with the new params.
  // No-op if new params are identical to the current `input_params_` and no
  // errors have been encountered.
  void OnSetFormat(const media::AudioParameters& params) override;

  // May buffer or immediately deliver `input_bus` to `mojo_encoder_`. If we
  // encounter an error or are `paused_`, input will be ignored. This will
  // return before `on_encoded_audio_cb_` is run.
  void EncodeAudio(std::unique_ptr<media::AudioBus> input_bus,
                   base::TimeTicks capture_time) override;

  // Must be called on `encoder_thread_` before destructing this class.
  void Shutdown() override;

 private:
  ~AudioTrackMojoEncoder() override;

  // Run when the platform encoder finishes initializing, will flush
  // `input_queue_`.
  void OnInitializeDone(media::EncoderStatus status);

  // Run when input is delivered to the platform encoder, or when an error is
  // encountered.
  void OnEncodeDone(media::EncoderStatus status);
  void OnEncodeOutput(
      media::EncodedAudioBuffer encoded_buffer,
      absl::optional<media::AudioEncoder::CodecDescription> codec_desc);

  // The `media::AudioEncoder` interface requires the callback provided to
  // `Initialize` to be run before any further calls are made. So, we store any
  // input we get while waiting for the callback in `input_queue_` and will
  // deliver it in `OnInitializeDone`.
  struct PendingData {
    std::unique_ptr<media::AudioBus> audio_bus;
    const base::TimeTicks capture_time;
  };

  AudioTrackRecorder::CodecId codec_;

  // Target bitrate. An optional parameter for the `mojo_encoder_`;
  const uint32_t bits_per_second_;
  media::EncoderStatus current_status_;
  std::unique_ptr<media::AudioEncoder> mojo_encoder_;
  base::queue<PendingData> input_queue_;

  base::WeakPtrFactory<AudioTrackMojoEncoder> weak_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIARECORDER_AUDIO_TRACK_MOJO_ENCODER_H_
