// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_AUDIO_CODECS_H_
#define MEDIA_BASE_AUDIO_CODECS_H_

#include <string>
#include <string_view>

#include "media/base/media_export.h"
#include "media/media_buildflags.h"

namespace media {

enum class AudioCodec {
  // These values are histogrammed over time; do not change their ordinal
  // values.  When deleting a codec replace it with a dummy value; when adding a
  // codec, do so at the bottom before kMaxValue, and update the value of
  // kMaxValue to equal the new codec.
  kUnknown = 0,
  kAAC = 1,
  kMP3 = 2,
  kPCM = 3,
  kVorbis = 4,
  kFLAC = 5,
  kAMR_NB = 6,
  kAMR_WB = 7,
  kPCM_MULAW = 8,
  kGSM_MS = 9,
  kPCM_S16BE = 10,
  kPCM_S24BE = 11,
  kOpus = 12,
  kEAC3 = 13,
  kPCM_ALAW = 14,
  kALAC = 15,
  kAC3 = 16,
  kMpegHAudio = 17,
  kDTS = 18,
  kDTSXP2 = 19,
  kDTSE = 20,
  kAC4 = 21,
  kIAMF = 22,
  // DO NOT ADD RANDOM AUDIO CODECS!
  //
  // The only acceptable time to add a new codec is if there is production code
  // that uses said codec in the same CL.

  // Must always be equal to the largest entry ever logged.
  kMaxValue = kIAMF,
};

enum class AudioCodecProfile {
  // These values are histogrammed over time; do not change their ordinal
  // values.  When deleting a profile replace it with a dummy value; when adding
  // a profile, do so at the bottom before kMaxValue, and update the value of
  // kMaxValue to equal the new codec.
  kUnknown = 0,
  kXHE_AAC = 1,
  kIAMF_SIMPLE = 2,
  kIAMF_BASE = 3,
  kMaxValue = kIAMF_BASE,
};

std::string MEDIA_EXPORT GetCodecName(AudioCodec codec);
std::string MEDIA_EXPORT GetProfileName(AudioCodecProfile profile);

MEDIA_EXPORT std::ostream& operator<<(std::ostream& os,
                                      const AudioCodec& codec);
MEDIA_EXPORT AudioCodec StringToAudioCodec(const std::string& codec_id);
#if BUILDFLAG(ENABLE_PLATFORM_AC4_AUDIO)
MEDIA_EXPORT bool ParseDolbyAc4CodecId(const std::string& codec_id,
                                       uint8_t* bitstream_version,
                                       uint8_t* presentation_version,
                                       uint8_t* presentation_level);
#endif  // BUILDFLAG(ENABLE_PLATFORM_AC4_AUDIO)
#if BUILDFLAG(ENABLE_PLATFORM_IAMF_AUDIO)
MEDIA_EXPORT bool ParseIamfCodecId(std::string_view codec_id,
                                   uint8_t* primary_profilec,
                                   uint8_t* additional_profilec);
#endif  // BUILDFLAG(ENABLE_PLATFORM_IAMF_AUDIO)

}  // namespace media

#endif  // MEDIA_BASE_AUDIO_CODECS_H_
