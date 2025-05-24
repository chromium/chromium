// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediarecorder/audio_track_pcm_encoder.h"

#include <optional>

#include "base/containers/heap_array.h"
#include "base/logging.h"
#include "media/base/audio_sample_types.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/decoder_buffer.h"

namespace blink {

AudioTrackPcmEncoder::AudioTrackPcmEncoder(
    OnEncodedAudioCB on_encoded_audio_cb,
    OnEncodedAudioErrorCB on_encoded_audio_error_cb)
    : AudioTrackEncoder(std::move(on_encoded_audio_cb),
                        std::move(on_encoded_audio_error_cb)) {}

void AudioTrackPcmEncoder::OnSetFormat(
    const media::AudioParameters& input_params) {
  DVLOG(1) << __func__
           << ", |input_params_|: " << input_params_.AsHumanReadableString();

  if (!input_params.IsValid()) {
    DLOG(ERROR) << "Invalid params: " << input_params.AsHumanReadableString();
    if (!on_encoded_audio_error_cb_.is_null()) {
      std::move(on_encoded_audio_error_cb_)
          .Run(media::EncoderStatus::Codes::kEncoderUnsupportedConfig);
    }
    return;
  }

  input_params_ = input_params;
}

void AudioTrackPcmEncoder::EncodeAudio(
    std::unique_ptr<media::AudioBus> input_bus,
    base::TimeTicks capture_time) {
  DVLOG(3) << __func__ << ", #frames " << input_bus->frames();
  DCHECK_EQ(input_bus->channels(), input_params_.channels());
  DCHECK(!capture_time.is_null());

  if (paused_)
    return;

  auto encoded_data = base::HeapArray<uint8_t>::Uninit(
      input_bus->frames() * input_bus->channels() * sizeof(float));

  input_bus->ToInterleaved<media::Float32SampleTypeTraits>(
      input_bus->frames(), reinterpret_cast<float*>(encoded_data.data()));

  const base::TimeTicks capture_time_of_first_sample =
      capture_time - media::AudioTimestampHelper::FramesToTime(
                         input_bus->frames(), input_params_.sample_rate());

  auto buffer = media::DecoderBuffer::FromArray(std::move(encoded_data));

  on_encoded_audio_cb_.Run(input_params_, std::move(buffer), std::nullopt,
                           capture_time_of_first_sample);
}

}  // namespace blink
