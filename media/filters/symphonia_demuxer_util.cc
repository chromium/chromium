// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/symphonia_demuxer_util.h"

namespace media {

AudioCodec SymphoniaCodecToAudioCodec(SymphoniaCodec codec) {
  switch (codec) {
    case SymphoniaCodec::Aac:
      return AudioCodec::kAAC;
    case SymphoniaCodec::Flac:
      return AudioCodec::kFLAC;
    case SymphoniaCodec::Mp3:
      return AudioCodec::kMP3;
    case SymphoniaCodec::Opus:
      return AudioCodec::kOpus;
    case SymphoniaCodec::Vorbis:
      return AudioCodec::kVorbis;
    case SymphoniaCodec::PcmAlaw:
      return AudioCodec::kPCM_ALAW;
    case SymphoniaCodec::PcmMulaw:
      return AudioCodec::kPCM_MULAW;
    case SymphoniaCodec::PcmS16be:
      return AudioCodec::kPCM_S16BE;
    case SymphoniaCodec::PcmS24be:
      return AudioCodec::kPCM_S24BE;
    case SymphoniaCodec::PcmF32:
    case SymphoniaCodec::PcmF32Planar:
    case SymphoniaCodec::PcmS16:
    case SymphoniaCodec::PcmS16Planar:
    case SymphoniaCodec::PcmS24:
    case SymphoniaCodec::PcmS32:
    case SymphoniaCodec::PcmS32Planar:
    case SymphoniaCodec::PcmU8:
    case SymphoniaCodec::PcmU8Planar:
      return AudioCodec::kPCM;
    default:
      return AudioCodec::kUnknown;
  }
}

SampleFormat SymphoniaCodecToSampleFormat(SymphoniaCodec codec) {
  switch (codec) {
    case SymphoniaCodec::PcmU8:
      return kSampleFormatU8;
    case SymphoniaCodec::PcmU8Planar:
      return kSampleFormatPlanarU8;
    case SymphoniaCodec::PcmS16:
    case SymphoniaCodec::PcmS16be:
      return kSampleFormatS16;
    case SymphoniaCodec::PcmS16Planar:
      return kSampleFormatPlanarS16;
    case SymphoniaCodec::PcmS24:
    case SymphoniaCodec::PcmS24be:
      return kSampleFormatS24;
    case SymphoniaCodec::PcmS32:
      return kSampleFormatS32;
    case SymphoniaCodec::PcmS32Planar:
      return kSampleFormatPlanarS32;
    case SymphoniaCodec::PcmF32:
      return kSampleFormatF32;
    case SymphoniaCodec::PcmF32Planar:
      return kSampleFormatPlanarF32;

    // Compressed audio decoded to S16 by default in Symphonia decoder.
    case SymphoniaCodec::Flac:
    case SymphoniaCodec::Mp3:
    case SymphoniaCodec::Vorbis:
    case SymphoniaCodec::Aac:
    case SymphoniaCodec::Opus:
      return kSampleFormatS16;

    // G.711 A-law and mu-law are 8-bit log-companded samples on the wire,
    // but `AudioDecoderConfig::sample_format()` describes the decoded output
    // format. Both FFmpeg and Symphonia (`symphonia-codec-pcm`) expand them
    // to 16-bit signed linear PCM (`kSampleFormatS16`), and Symphonia
    // rejects `bits_per_sample != 16` for these codecs.
    case SymphoniaCodec::PcmAlaw:
    case SymphoniaCodec::PcmMulaw:
      return kSampleFormatS16;

    default:
      return kUnknownSampleFormat;
  }
}

container_names::MediaContainerName SymphoniaFormatNameToContainerName(
    std::string_view name) {
  if (name == "wave") {
    return container_names::MediaContainerName::kContainerWAV;
  } else if (name == "ogg") {
    return container_names::MediaContainerName::kContainerOgg;
  } else if (name == "flac") {
    return container_names::MediaContainerName::kContainerFLAC;
  } else if (name == "mp1" || name == "mp2" || name == "mp3") {
    return container_names::MediaContainerName::kContainerMP3;
  } else if (name == "aiff") {
    return container_names::MediaContainerName::kContainerAIFF;
  } else if (name == "isomp4") {
    return container_names::MediaContainerName::kContainerMOV;
  } else if (name == "matroska") {
    return container_names::MediaContainerName::kContainerWEBM;
  }
  return container_names::MediaContainerName::kContainerUnknown;
}

}  // namespace media
