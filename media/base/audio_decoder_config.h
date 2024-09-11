// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_AUDIO_DECODER_CONFIG_H_
#define MEDIA_BASE_AUDIO_DECODER_CONFIG_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/time/time.h"
#include "media/base/audio_codecs.h"
#include "media/base/channel_layout.h"
#include "media/base/encryption_scheme.h"
#include "media/base/media_export.h"
#include "media/base/sample_format.h"

namespace media {

class MEDIA_EXPORT AudioDecoderConfig {
 public:
  // Constructs an uninitialized object. Clients should call Initialize() with
  // appropriate values before using.
  AudioDecoderConfig();

  // Constructs an initialized object.
  AudioDecoderConfig(AudioCodec codec,
                     SampleFormat sample_format,
                     ChannelLayout channel_layout,
                     int samples_per_second,
                     const std::vector<uint8_t>& extra_data,
                     EncryptionScheme encryption_scheme);

  AudioDecoderConfig(const AudioDecoderConfig& other);
  AudioDecoderConfig(AudioDecoderConfig&& other);
  AudioDecoderConfig& operator=(const AudioDecoderConfig& other);
  AudioDecoderConfig& operator=(AudioDecoderConfig&& other);

  ~AudioDecoderConfig();

  // Resets the internal state of this object. |codec_delay| is in frames.
  void Initialize(AudioCodec codec,
                  SampleFormat sample_format,
                  ChannelLayout channel_layout,
                  int samples_per_second,
                  const std::vector<uint8_t>& extra_data,
                  EncryptionScheme encryption_scheme,
                  base::TimeDelta seek_preroll,
                  int codec_delay);

  // Returns true if this object has appropriate configuration values, false
  // otherwise.
  bool IsValidConfig() const;

  // Returns true if all fields in |config| match this config.
  // Note: The contents of |extra_data_| are compared not the raw pointers.
  bool Matches(const AudioDecoderConfig& config) const;

  // Returns a human-readable string describing |*this|.
  std::string AsHumanReadableString() const;

  // Sets the number of channels if |channel_layout_| is CHANNEL_LAYOUT_DISCRETE
  void SetChannelsForDiscrete(int channels);

  AudioCodec codec() const { return codec_; }
  int bytes_per_channel() const { return bytes_per_channel_; }
  ChannelLayout channel_layout() const { return channel_layout_; }
  int channels() const { return channels_; }
  int samples_per_second() const { return samples_per_second_; }
  SampleFormat sample_format() const { return sample_format_; }
  int bytes_per_frame() const { return bytes_per_frame_; }
  base::TimeDelta seek_preroll() const { return seek_preroll_; }
  int codec_delay() const { return codec_delay_; }

  // Optional byte data required to initialize audio decoders such as Vorbis
  // codebooks or AAC AudioSpecificConfig.
  const std::vector<uint8_t>& extra_data() const { return extra_data_; }

  // Whether the audio stream is potentially encrypted.
  // Note that in a potentially encrypted audio stream, individual buffers
  // can be encrypted or not encrypted.
  bool is_encrypted() const {
    return encryption_scheme_ != EncryptionScheme::kUnencrypted;
  }

  // Encryption scheme used for encrypted buffers.
  EncryptionScheme encryption_scheme() const { return encryption_scheme_; }

  // Sets the config to be encrypted or not encrypted manually. This can be
  // useful for decryptors that decrypts an encrypted stream to a clear stream.
  void SetIsEncrypted(bool is_encrypted);

  // Optionally set if the AudioCodec has a profile which may preclude certain
  // decoders from having support.
  void set_profile(AudioCodecProfile profile) { profile_ = profile; }
  AudioCodecProfile profile() const { return profile_; }

  bool should_discard_decoder_delay() const {
    return should_discard_decoder_delay_;
  }
  void disable_discard_decoder_delay() {
    should_discard_decoder_delay_ = false;
  }

  // Optionally set by renderer to provide hardware layout when playback
  // starts. Intentionally not part of IsValid(). Layout is not updated for
  // device changes - use with care!
  void set_target_output_channel_layout(ChannelLayout output_layout) {
    target_output_channel_layout_ = output_layout;
  }
  ChannelLayout target_output_channel_layout() const {
    return target_output_channel_layout_;
  }

  // Optionally set by renderer to signal desired bitstream-passthru format.
  void set_target_output_sample_format(SampleFormat sample_format) {
    target_output_sample_format_ = sample_format;
  }
  SampleFormat target_output_sample_format() const {
    return target_output_sample_format_;
  }

  void set_aac_extra_data(std::vector<uint8_t> aac_extra_data) {
    aac_extra_data_ = std::move(aac_extra_data);
  }
  const std::vector<uint8_t>& aac_extra_data() const { return aac_extra_data_; }

 private:
  // WARNING: When modifying or adding any parameters, update the following:
  // - AudioDecoderConfig::AsHumanReadableString()
  // - AudioDecoderConfig::Matches()
  // - media::mojom::AudioDecoderConfig
  // - audio_decoder_config_mojom_traits.{h|cc}
  // - audio_decoder_config_mojom_traits_unittest.cc

  // Mandatory parameters passed in constructor:

  AudioCodec codec_ = AudioCodec::kUnknown;
  SampleFormat sample_format_ = kUnknownSampleFormat;
  ChannelLayout channel_layout_ = CHANNEL_LAYOUT_UNSUPPORTED;
  int samples_per_second_ = 0;
  std::vector<uint8_t> extra_data_;
  EncryptionScheme encryption_scheme_ = EncryptionScheme::kUnencrypted;

  // The duration of data that the decoder must decode before the decoded data
  // is valid.
  base::TimeDelta seek_preroll_;

  // The number of frames the decoder should discard before returning decoded
  // data. Can include both decoder delay and padding added during encoding.
  int codec_delay_ = 0;

  // Optional parameters that can be set later:

  AudioCodecProfile profile_ = AudioCodecProfile::kUnknown;

  // Layout of the output hardware. Optionally set. See setter comments.
  ChannelLayout target_output_channel_layout_ = CHANNEL_LAYOUT_NONE;

  // Desired output format of bitstream. Optionally set. See setter comments.
  SampleFormat target_output_sample_format_ = kUnknownSampleFormat;

  // This is a hack for backward compatibility. For AAC, to preserve existing
  // behavior, we set `aac_extra_data_` on all platforms but only set
  // `extra_data` on Android.
  // TODO(crbug.com/40198159): Remove this after we land a long term fix.
  std::vector<uint8_t> aac_extra_data_;

  // Indicates if a decoder should implicitly discard decoder delay without it
  // being explicitly marked in discard padding.
  bool should_discard_decoder_delay_ = true;

  // Derived values from mandatory and optional parameters above.
  // A frame contains samples across all channels.

  int bytes_per_channel_ = 0;
  int bytes_per_frame_ = 0;

  // Count of channels. By default derived from `channel_layout_`, but can also
  // be manually set in `SetChannelsForDiscrete()`;
  int channels_ = 0;

  // Not using DISALLOW_COPY_AND_ASSIGN here intentionally to allow the compiler
  // generated copy constructor and assignment operator. Since the extra data is
  // typically small, the performance impact is minimal.
};

}  // namespace media

#endif  // MEDIA_BASE_AUDIO_DECODER_CONFIG_H_
