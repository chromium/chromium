// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_codecs.h"

#include "base/logging.h"
#include "base/strings/string_util.h"

namespace media {

// These names come from src/third_party/ffmpeg/libavcodec/codec_desc.c
std::string GetCodecName(AudioCodec codec) {
  switch (codec) {
    case kUnknownAudioCodec:
      return "unknown";
    case kCodecAAC:
      return "aac";
    case kCodecMP3:
      return "mp3";
    case kCodecPCM:
    case kCodecPCM_S16BE:
    case kCodecPCM_S24BE:
      return "pcm";
    case kCodecVorbis:
      return "vorbis";
    case kCodecFLAC:
      return "flac";
    case kCodecAMR_NB:
      return "amr_nb";
    case kCodecAMR_WB:
      return "amr_wb";
    case kCodecPCM_MULAW:
      return "pcm_mulaw";
    case kCodecGSM_MS:
      return "gsm_ms";
    case kCodecOpus:
      return "opus";
    case kCodecPCM_ALAW:
      return "pcm_alaw";
    case kCodecEAC3:
      return "eac3";
    case kCodecALAC:
      return "alac";
    case kCodecAC3:
      return "ac3";
    case kCodecMpegHAudio:
      return "mpeg-h-audio";
  }
  NOTREACHED();
  return "";
}

AudioCodec StringToAudioCodec(const std::string& codec_id) {
  if (codec_id == "aac")
    return kCodecAAC;
  if (codec_id == "ac-3" || codec_id == "mp4a.A5" || codec_id == "mp4a.a5")
    return kCodecAC3;
  if (codec_id == "ec-3" || codec_id == "mp4a.A6" || codec_id == "mp4a.a6")
    return kCodecEAC3;
  if (codec_id == "mp3" || codec_id == "mp4a.69" || codec_id == "mp4a.6B")
    return kCodecMP3;
  if (codec_id == "alac")
    return kCodecALAC;
  if (codec_id == "flac")
    return kCodecFLAC;
  if (base::StartsWith(codec_id, "mhm1.", base::CompareCase::SENSITIVE) ||
      base::StartsWith(codec_id, "mha1.", base::CompareCase::SENSITIVE)) {
    return kCodecMpegHAudio;
  }
  if (codec_id == "opus")
    return kCodecOpus;
  if (codec_id == "vorbis")
    return kCodecVorbis;
  if (base::StartsWith(codec_id, "mp4a.40.", base::CompareCase::SENSITIVE))
    return kCodecAAC;
  return kUnknownAudioCodec;
}

}  // namespace media
