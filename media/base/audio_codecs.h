// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_AUDIO_CODECS_H_
#define MEDIA_BASE_AUDIO_CODECS_H_

#include <string>
#include "media/base/media_export.h"

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
  // DO NOT ADD RANDOM AUDIO CODECS!
  //
  // The only acceptable time to add a new codec is if there is production code
  // that uses said codec in the same CL.

  // Must always be equal to the largest entry ever logged.
  kMaxValue = kDTSXP2,
};

enum class AudioCodecProfile {
  // These values are histogrammed over time; do not change their ordinal
  // values.  When deleting a profile replace it with a dummy value; when adding
  // a profile, do so at the bottom before kMaxValue, and update the value of
  // kMaxValue to equal the new codec.
  kUnknown = 0,
  kXHE_AAC = 1,
  kMaxValue = kXHE_AAC,
};

std::string MEDIA_EXPORT GetCodecName(AudioCodec codec);
std::string MEDIA_EXPORT GetProfileName(AudioCodecProfile profile);

MEDIA_EXPORT std::ostream& operator<<(std::ostream& os,
                                      const AudioCodec& codec);
MEDIA_EXPORT AudioCodec StringToAudioCodec(const std::string& codec_id);

}  // namespace media

#endif  // MEDIA_BASE_AUDIO_CODECS_H_
