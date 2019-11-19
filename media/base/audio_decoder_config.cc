// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_decoder_config.h"

#include "base/logging.h"
#include "media/base/limits.h"
#include "media/base/media_util.h"

namespace media {

AudioDecoderConfig::AudioDecoderConfig() {}

AudioDecoderConfig::AudioDecoderConfig(AudioCodec codec,
                                       SampleFormat sample_format,
                                       ChannelLayout channel_layout,
                                       int samples_per_second,
                                       const std::vector<uint8_t>& extra_data,
                                       EncryptionScheme encryption_scheme) {
  Initialize(codec, sample_format, channel_layout, samples_per_second,
             extra_data, encryption_scheme, base::TimeDelta(), 0);
}

AudioDecoderConfig::AudioDecoderConfig(const AudioDecoderConfig& other) =
    default;

void AudioDecoderConfig::Initialize(AudioCodec codec,
                                    SampleFormat sample_format,
                                    ChannelLayout channel_layout,
                                    int samples_per_second,
                                    const std::vector<uint8_t>& extra_data,
                                    EncryptionScheme encryption_scheme,
                                    base::TimeDelta seek_preroll,
                                    int codec_delay) {
  codec_ = codec;
  channel_layout_ = channel_layout;
  samples_per_second_ = samples_per_second;
  sample_format_ = sample_format;
  bytes_per_channel_ = SampleFormatToBytesPerChannel(sample_format);
  extra_data_ = extra_data;
  encryption_scheme_ = encryption_scheme;
  seek_preroll_ = seek_preroll;
  codec_delay_ = codec_delay;

  // If |channel_layout_| is CHANNEL_LAYOUT_DISCRETE, |channels_| and
  // |bytes_per_frame_| will be overwritten in SetChannelsForDiscrete()
  channels_ = ChannelLayoutToChannelCount(channel_layout_);
  bytes_per_frame_ = channels_ * bytes_per_channel_;

  should_discard_decoder_delay_ = true;
}

AudioDecoderConfig::~AudioDecoderConfig() = default;

bool AudioDecoderConfig::IsValidConfig() const {
  return codec_ != kUnknownAudioCodec &&
         channel_layout_ != CHANNEL_LAYOUT_UNSUPPORTED &&
         bytes_per_channel_ > 0 &&
         bytes_per_channel_ <= limits::kMaxBytesPerSample &&
         samples_per_second_ > 0 &&
         samples_per_second_ <= limits::kMaxSampleRate &&
         sample_format_ != kUnknownSampleFormat &&
         seek_preroll_ >= base::TimeDelta() && codec_delay_ >= 0;
}

bool AudioDecoderConfig::Matches(const AudioDecoderConfig& config) const {
  return ((codec() == config.codec()) &&
          (bytes_per_channel() == config.bytes_per_channel()) &&
          (channel_layout() == config.channel_layout()) &&
          (samples_per_second() == config.samples_per_second()) &&
          (extra_data() == config.extra_data()) &&
          (encryption_scheme() == config.encryption_scheme()) &&
          (sample_format() == config.sample_format()) &&
          (seek_preroll() == config.seek_preroll()) &&
          (codec_delay() == config.codec_delay()) &&
          (should_discard_decoder_delay() ==
           config.should_discard_decoder_delay()));
}

std::string AudioDecoderConfig::AsHumanReadableString() const {
  std::ostringstream s;
  s << "codec: " << GetCodecName(codec())
    << ", bytes_per_channel: " << bytes_per_channel()
    << ", channel_layout: " << ChannelLayoutToString(channel_layout())
    << ", channels: " << channels()
    << ", samples_per_second: " << samples_per_second()
    << ", sample_format: " << SampleFormatToString(sample_format())
    << ", bytes_per_frame: " << bytes_per_frame()
    << ", seek_preroll: " << seek_preroll().InMicroseconds() << "us"
    << ", codec_delay: " << codec_delay()
    << ", has extra data: " << (extra_data().empty() ? "false" : "true")
    << ", encryption scheme: " << encryption_scheme()
    << ", discard decoder delay: "
    << (should_discard_decoder_delay() ? "true" : "false");
  return s.str();
}

void AudioDecoderConfig::SetChannelsForDiscrete(int channels) {
  DCHECK(channel_layout_ == CHANNEL_LAYOUT_DISCRETE ||
         channels == ChannelLayoutToChannelCount(channel_layout_));
  if (channels <= 0 || channels >= limits::kMaxChannels) {
    DVLOG(1) << __func__ << ": Unsupported number of channels: " << channels;
    return;
  }
  channels_ = channels;
  bytes_per_frame_ = channels_ * bytes_per_channel_;
}

void AudioDecoderConfig::SetIsEncrypted(bool is_encrypted) {
  if (!is_encrypted) {
    DCHECK_NE(encryption_scheme_, EncryptionScheme::kUnencrypted)
        << "Config is already clear.";
    encryption_scheme_ = EncryptionScheme::kUnencrypted;
  } else {
    DCHECK_EQ(encryption_scheme_, EncryptionScheme::kUnencrypted)
        << "Config is already encrypted.";
    // TODO(xhwang): This is only used to guide decoder selection, so set
    // a common encryption scheme that should be supported by all decrypting
    // decoders. We should be able to remove this when we support switching
    // decoders at run time. See http://crbug.com/695595
    encryption_scheme_ = EncryptionScheme::kCenc;
  }
}

}  // namespace media
