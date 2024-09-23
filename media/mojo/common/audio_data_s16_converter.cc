// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/mojo/common/audio_data_s16_converter.h"

#include <memory>

#include "media/base/audio_buffer.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/channel_layout.h"
#include "media/base/channel_mixer.h"
#include "media/mojo/mojom/audio_data.mojom.h"
#include "media/mojo/mojom/media_types.mojom.h"

namespace media {

AudioDataS16Converter::AudioDataS16Converter() = default;
AudioDataS16Converter::~AudioDataS16Converter() = default;

mojom::AudioDataS16Ptr AudioDataS16Converter::ConvertToAudioDataS16(
    scoped_refptr<AudioBuffer> buffer,
    bool is_multichannel_supported) {
  DCHECK_GT(buffer->frame_count(), 0);
  DCHECK_GT(buffer->channel_count(), 0);
  DCHECK_GT(buffer->sample_rate(), 0);

  // If the audio is already in the interleaved signed int 16 format, directly
  // assign it to the buffer, unless it is multichannel when multichannel is
  // not supported.
  if (buffer->sample_format() == SampleFormat::kSampleFormatS16 &&
      (buffer->channel_count() == 1 || is_multichannel_supported)) {
    auto signed_buffer = mojom::AudioDataS16::New();
    signed_buffer->channel_count = buffer->channel_count();
    signed_buffer->frame_count = buffer->frame_count();
    signed_buffer->sample_rate = buffer->sample_rate();
    int16_t* audio_data = reinterpret_cast<int16_t*>(buffer->channel_data()[0]);
    signed_buffer->data.assign(
        audio_data,
        audio_data + buffer->frame_count() * buffer->channel_count());
    return signed_buffer;
  }

  CopyBufferToTempAudioBus(*buffer);
  return ConvertToAudioDataS16(*temp_audio_bus_, buffer->sample_rate(),
                               buffer->channel_layout(),
                               is_multichannel_supported);
}

mojom::AudioDataS16Ptr AudioDataS16Converter::ConvertToAudioDataS16(
    std::unique_ptr<AudioBus> audio_bus,
    int sample_rate,
    ChannelLayout channel_layout,
    bool is_multichannel_supported) {
  DCHECK_GT(audio_bus->frames(), 0);
  DCHECK_GT(audio_bus->channels(), 0);
  return ConvertToAudioDataS16(*audio_bus, sample_rate, channel_layout,
                               is_multichannel_supported);
}

mojom::AudioDataS16Ptr AudioDataS16Converter::ConvertToAudioDataS16(
    const AudioBus& audio_bus,
    int sample_rate,
    ChannelLayout channel_layout,
    bool is_multichannel_supported) {
  auto signed_buffer = mojom::AudioDataS16::New();
  signed_buffer->channel_count = audio_bus.channels();
  signed_buffer->frame_count = audio_bus.frames();
  signed_buffer->sample_rate = sample_rate;

  // If multichannel audio is not supported, mix the channels into a monaural
  // channel before converting it.
  if (audio_bus.channels() > 1 && !is_multichannel_supported) {
    signed_buffer->channel_count = 1;

    ResetChannelMixerIfNeeded(audio_bus.frames(), channel_layout,
                              audio_bus.channels());
    signed_buffer->data.resize(audio_bus.frames());

    channel_mixer_->Transform(&audio_bus, monaural_audio_bus_.get());
    monaural_audio_bus_->ToInterleaved<SignedInt16SampleTypeTraits>(
        monaural_audio_bus_->frames(), &signed_buffer->data[0]);

    return signed_buffer;
  }

  signed_buffer->data.resize(audio_bus.frames() * audio_bus.channels());
  audio_bus.ToInterleaved<SignedInt16SampleTypeTraits>(audio_bus.frames(),
                                                       &signed_buffer->data[0]);

  return signed_buffer;
}

void AudioDataS16Converter::CopyBufferToTempAudioBus(
    const AudioBuffer& buffer) {
  if (!temp_audio_bus_ ||
      buffer.channel_count() != temp_audio_bus_->channels() ||
      buffer.frame_count() != temp_audio_bus_->frames()) {
    temp_audio_bus_ =
        AudioBus::Create(buffer.channel_count(), buffer.frame_count());
  }

  buffer.ReadFrames(buffer.frame_count(),
                    /*source_frame_offset*/ 0, /*dest_frame_offset*/ 0,
                    temp_audio_bus_.get());
}

void AudioDataS16Converter::ResetChannelMixerIfNeeded(
    int frame_count,
    ChannelLayout channel_layout,
    int channel_count) {
  if (!monaural_audio_bus_ || frame_count != monaural_audio_bus_->frames()) {
    monaural_audio_bus_ = AudioBus::Create(1 /*channels*/, frame_count);
  }

  if (channel_layout != channel_layout_ || channel_count != channel_count_) {
    channel_layout_ = channel_layout;
    channel_count_ = channel_count;
    channel_mixer_ = std::make_unique<ChannelMixer>(
        channel_layout, channel_count, CHANNEL_LAYOUT_MONO,
        1 /*output_channels*/);
  }
}

}  // namespace media
