// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediarecorder/audio_track_pcm_encoder.h"

#include "base/stl_util.h"
#include "media/base/audio_sample_types.h"
#include "media/base/audio_timestamp_helper.h"

namespace blink {

AudioTrackPcmEncoder::AudioTrackPcmEncoder(OnEncodedAudioCB on_encoded_audio_cb)
    : AudioTrackEncoder(std::move(on_encoded_audio_cb)) {}

void AudioTrackPcmEncoder::OnSetFormat(
    const media::AudioParameters& input_params) {
  DVLOG(1) << __func__
           << ", |input_params_|: " << input_params_.AsHumanReadableString();
  DCHECK_CALLED_ON_VALID_THREAD(encoder_thread_checker_);

  if (!input_params.IsValid()) {
    DLOG(ERROR) << "Invalid params: " << input_params.AsHumanReadableString();
    return;
  }
  input_params_ = input_params;
}

void AudioTrackPcmEncoder::EncodeAudio(
    std::unique_ptr<media::AudioBus> input_bus,
    base::TimeTicks capture_time) {
  DVLOG(3) << __func__ << ", #frames " << input_bus->frames();
  DCHECK_CALLED_ON_VALID_THREAD(encoder_thread_checker_);
  DCHECK_EQ(input_bus->channels(), input_params_.channels());
  DCHECK(!capture_time.is_null());

  if (paused_)
    return;

  std::string encoded_data_string;
  encoded_data_string.resize(input_bus->frames() * input_bus->channels() *
                             sizeof(float));
  char* encoded_data_ptr = base::data(encoded_data_string);
  input_bus->ToInterleaved<media::Float32SampleTypeTraits>(
      input_bus->frames(), reinterpret_cast<float*>(encoded_data_ptr));

  const base::TimeTicks capture_time_of_first_sample =
      capture_time - media::AudioTimestampHelper::FramesToTime(
                         input_bus->frames(), input_params_.sample_rate());
  on_encoded_audio_cb_.Run(input_params_, std::move(encoded_data_string),
                           capture_time_of_first_sample);
}

}  // namespace blink
