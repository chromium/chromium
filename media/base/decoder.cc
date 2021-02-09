// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/decoder.h"

namespace media {

Decoder::Decoder() = default;

Decoder::~Decoder() = default;

bool Decoder::IsPlatformDecoder() const {
  return false;
}

bool Decoder::SupportsDecryption() const {
  return false;
}

std::string GetDecoderName(VideoDecoderType type) {
  switch (type) {
    case VideoDecoderType::kUnknown:
      return "Unknown Video Decoder";
    case VideoDecoderType::kFFmpeg:
      return "FFmpegVideoDecoder";
    case VideoDecoderType::kVpx:
      return "VpxVideoDecoder";
    case VideoDecoderType::kAom:
      return "AomVideoDecoder";
    case VideoDecoderType::kMojo:
      return "MojoVideoDecoder";
    case VideoDecoderType::kDecrypting:
      return "DecryptingVideoDecoder";
    case VideoDecoderType::kDav1d:
      return "Dav1dVideoDecoder";
    case VideoDecoderType::kFuchsia:
      return "FuchsiaVideoDecoder";
    case VideoDecoderType::kMediaCodec:
      return "MediaCodecVideoDecoder";
    case VideoDecoderType::kGav1:
      return "Gav1VideoDecoder";
    case VideoDecoderType::kD3D11:
      return "D3D11VideoDecoder";
    case VideoDecoderType::kVaapi:
      return "VaapiVideoDecodeAccelerator";
    case VideoDecoderType::kBroker:
      return "VideoDecoderBroker";
    case VideoDecoderType::kChromeOs:
      return "VideoDecoderPipeline (ChromeOs)";
    case VideoDecoderType::kVda:
      return "VideoDecodeAccelerator";
  }
}

std::string GetDecoderName(AudioDecoderType type) {
  switch (type) {
    case AudioDecoderType::kUnknown:
      return "Unknown Audio Decoder";
    case AudioDecoderType::kFFmpeg:
      return "FFmpegAudioDecoder";
    case AudioDecoderType::kMojo:
      return "MojoAudioDecoder";
    case AudioDecoderType::kDecrypting:
      return "DecryptingAudioDecoder";
    case AudioDecoderType::kMediaCodec:
      return "MediaCodecAudioDecoder";
    case AudioDecoderType::kBroker:
      return "AudioDecoderBroker";
  }
}

}  // namespace media
