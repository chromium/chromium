// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/audio_codec_factory.h"

#include <memory>
#include <vector>

#include "third_party/webrtc/api/audio_codecs/L16/audio_decoder_L16.h"
#include "third_party/webrtc/api/audio_codecs/L16/audio_encoder_L16.h"
#include "third_party/webrtc/api/audio_codecs/audio_decoder_factory_template.h"
#include "third_party/webrtc/api/audio_codecs/audio_encoder_factory_template.h"
#include "third_party/webrtc/api/audio_codecs/g711/audio_decoder_g711.h"
#include "third_party/webrtc/api/audio_codecs/g711/audio_encoder_g711.h"
#include "third_party/webrtc/api/audio_codecs/g722/audio_decoder_g722.h"
#include "third_party/webrtc/api/audio_codecs/g722/audio_encoder_g722.h"
#include "third_party/webrtc/api/audio_codecs/isac/audio_decoder_isac.h"
#include "third_party/webrtc/api/audio_codecs/isac/audio_encoder_isac.h"
#include "third_party/webrtc/api/audio_codecs/opus/audio_decoder_multi_channel_opus.h"
#include "third_party/webrtc/api/audio_codecs/opus/audio_decoder_opus.h"
#include "third_party/webrtc/api/audio_codecs/opus/audio_encoder_multi_channel_opus.h"
#include "third_party/webrtc/api/audio_codecs/opus/audio_encoder_opus.h"

namespace blink {

namespace {

// Modify an audio encoder to not advertise support for anything.
template <typename T>
struct NotAdvertisedEncoder {
  using Config = typename T::Config;
  static absl::optional<Config> SdpToConfig(
      const webrtc::SdpAudioFormat& audio_format) {
    return T::SdpToConfig(audio_format);
  }
  static void AppendSupportedEncoders(
      std::vector<webrtc::AudioCodecSpec>* specs) {
    // Don't advertise support for anything.
  }
  static webrtc::AudioCodecInfo QueryAudioEncoder(const Config& config) {
    return T::QueryAudioEncoder(config);
  }
  static std::unique_ptr<webrtc::AudioEncoder> MakeAudioEncoder(
      const Config& config,
      int payload_type,
      absl::optional<webrtc::AudioCodecPairId> codec_pair_id) {
    return T::MakeAudioEncoder(config, payload_type, codec_pair_id);
  }
};

// Modify an audio decoder to not advertise support for anything.
template <typename T>
struct NotAdvertisedDecoder {
  using Config = typename T::Config;
  static absl::optional<Config> SdpToConfig(
      const webrtc::SdpAudioFormat& audio_format) {
    return T::SdpToConfig(audio_format);
  }
  static void AppendSupportedDecoders(
      std::vector<webrtc::AudioCodecSpec>* specs) {
    // Don't advertise support for anything.
  }
  static std::unique_ptr<webrtc::AudioDecoder> MakeAudioDecoder(
      const Config& config,
      absl::optional<webrtc::AudioCodecPairId> codec_pair_id) {
    return T::MakeAudioDecoder(config, codec_pair_id);
  }
};

}  // namespace

rtc::scoped_refptr<webrtc::AudioEncoderFactory>
CreateWebrtcAudioEncoderFactory() {
  return webrtc::CreateAudioEncoderFactory<
      webrtc::AudioEncoderOpus, webrtc::AudioEncoderIsac,
      webrtc::AudioEncoderG722, webrtc::AudioEncoderG711,
      NotAdvertisedEncoder<webrtc::AudioEncoderL16>,
      NotAdvertisedEncoder<webrtc::AudioEncoderMultiChannelOpus>>();
}

rtc::scoped_refptr<webrtc::AudioDecoderFactory>
CreateWebrtcAudioDecoderFactory() {
  return webrtc::CreateAudioDecoderFactory<
      webrtc::AudioDecoderOpus, webrtc::AudioDecoderIsac,
      webrtc::AudioDecoderG722, webrtc::AudioDecoderG711,
      NotAdvertisedDecoder<webrtc::AudioDecoderL16>,
      NotAdvertisedDecoder<webrtc::AudioDecoderMultiChannelOpus>>();
}

}  // namespace blink
