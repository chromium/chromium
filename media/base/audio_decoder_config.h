// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_AUDIO_DECODER_CONFIG_H_
#define MEDIA_BASE_AUDIO_DECODER_CONFIG_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/time/time.h"
#include "media/base/audio_codecs.h"
#include "media/base/channel_layout.h"
#include "media/base/encryption_scheme.h"
#include "media/base/media_export.h"
#include "media/base/sample_format.h"

namespace media {

// TODO(dalecurtis): FFmpeg API uses |bytes_per_channel| instead of
// |bits_per_channel|, we should switch over since bits are generally confusing
// to work with.
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
  int bits_per_channel() const { return bytes_per_channel_ * 8; }
  int bytes_per_channel() const { return bytes_per_channel_; }
  ChannelLayout channel_layout() const { return channel_layout_; }
  int channels() const { return channels_; }
  int samples_per_second() const { return samples_per_second_; }
  SampleFormat sample_format() const { return sample_format_; }
  int bytes_per_frame() const { return bytes_per_frame_; }
  base::TimeDelta seek_preroll() const { return seek_preroll_; }
  int codec_delay() const { return codec_delay_; }

  // Optional byte data required to initialize audio decoders such as Vorbis
  // codebooks.
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

 private:
  AudioCodec codec_ = kUnknownAudioCodec;
  SampleFormat sample_format_ = kUnknownSampleFormat;
  int bytes_per_channel_ = 0;
  int samples_per_second_ = 0;
  int bytes_per_frame_ = 0;
  std::vector<uint8_t> extra_data_;
  EncryptionScheme encryption_scheme_ = EncryptionScheme::kUnencrypted;

  // Layout and count of the *stream* being decoded.
  ChannelLayout channel_layout_ = CHANNEL_LAYOUT_UNSUPPORTED;
  int channels_ = 0;

  // Layout of the output hardware. Optionally set. See setter comments.
  ChannelLayout target_output_channel_layout_ = CHANNEL_LAYOUT_NONE;

  // |seek_preroll_| is the duration of the data that the decoder must decode
  // before the decoded data is valid.
  base::TimeDelta seek_preroll_;

  // |codec_delay_| is the number of frames the decoder should discard before
  // returning decoded data.  This value can include both decoder delay as well
  // as padding added during encoding.
  int codec_delay_ = 0;

  // Indicates if a decoder should implicitly discard decoder delay without it
  // being explicitly marked in discard padding.
  bool should_discard_decoder_delay_ = true;

  // Not using DISALLOW_COPY_AND_ASSIGN here intentionally to allow the compiler
  // generated copy constructor and assignment operator. Since the extra data is
  // typically small, the performance impact is minimal.
};

}  // namespace media

#endif  // MEDIA_BASE_AUDIO_DECODER_CONFIG_H_
