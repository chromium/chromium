// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_codecs.h"

#include <ostream>

#include "base/strings/string_util.h"

namespace media {

// These names come from src/third_party/ffmpeg/libavcodec/codec_desc.c
std::string GetCodecName(AudioCodec codec) {
  switch (codec) {
    case AudioCodec::kUnknown:
      return "unknown";
    case AudioCodec::kAAC:
      return "aac";
    case AudioCodec::kMP3:
      return "mp3";
    case AudioCodec::kPCM:
    case AudioCodec::kPCM_S16BE:
    case AudioCodec::kPCM_S24BE:
      return "pcm";
    case AudioCodec::kVorbis:
      return "vorbis";
    case AudioCodec::kFLAC:
      return "flac";
    case AudioCodec::kAMR_NB:
      return "amr_nb";
    case AudioCodec::kAMR_WB:
      return "amr_wb";
    case AudioCodec::kPCM_MULAW:
      return "pcm_mulaw";
    case AudioCodec::kGSM_MS:
      return "gsm_ms";
    case AudioCodec::kOpus:
      return "opus";
    case AudioCodec::kPCM_ALAW:
      return "pcm_alaw";
    case AudioCodec::kEAC3:
      return "eac3";
    case AudioCodec::kALAC:
      return "alac";
    case AudioCodec::kAC3:
      return "ac3";
    case AudioCodec::kMpegHAudio:
      return "mpeg-h-audio";
    case AudioCodec::kDTS:
      return "dts";
    case AudioCodec::kDTSXP2:
      return "dtsx-p2";
  }
}

std::string GetProfileName(AudioCodecProfile profile) {
  switch (profile) {
    case AudioCodecProfile::kUnknown:
      return "unknown";
    case AudioCodecProfile::kXHE_AAC:
      return "xhe-aac";
  }
}

AudioCodec StringToAudioCodec(const std::string& codec_id) {
  if (codec_id == "aac")
    return AudioCodec::kAAC;
  if (codec_id == "ac-3" || codec_id == "mp4a.A5" || codec_id == "mp4a.a5")
    return AudioCodec::kAC3;
  if (codec_id == "ec-3" || codec_id == "mp4a.A6" || codec_id == "mp4a.a6")
    return AudioCodec::kEAC3;
  if (codec_id == "dtsc")
    return AudioCodec::kDTS;
  if (codec_id == "dtsx")
    return AudioCodec::kDTSXP2;
  if (codec_id == "mp3" || codec_id == "mp4a.69" || codec_id == "mp4a.6B")
    return AudioCodec::kMP3;
  if (codec_id == "alac")
    return AudioCodec::kALAC;
  if (codec_id == "flac")
    return AudioCodec::kFLAC;
  if (base::StartsWith(codec_id, "mhm1.", base::CompareCase::SENSITIVE) ||
      base::StartsWith(codec_id, "mha1.", base::CompareCase::SENSITIVE)) {
    return AudioCodec::kMpegHAudio;
  }
  if (codec_id == "opus")
    return AudioCodec::kOpus;
  if (codec_id == "vorbis")
    return AudioCodec::kVorbis;
  if (codec_id == "dtsc")
    return AudioCodec::kDTS;
  if (codec_id == "dtsx")
    return AudioCodec::kDTSXP2;
  if (base::StartsWith(codec_id, "mp4a.40.", base::CompareCase::SENSITIVE))
    return AudioCodec::kAAC;
  return AudioCodec::kUnknown;
}

std::ostream& operator<<(std::ostream& os, const AudioCodec& codec) {
  return os << GetCodecName(codec);
}

}  // namespace media
