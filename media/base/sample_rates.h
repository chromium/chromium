// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_SAMPLE_RATES_H_
#define MEDIA_BASE_SAMPLE_RATES_H_

#include "media/base/media_export.h"

namespace media {

// Enumeration used for histogramming sample rates into distinct buckets.
// Logged to UMA, so never reuse a value, always add new/greater ones!
enum AudioSampleRate {
  k8000Hz = 0,
  k16000Hz = 1,
  k32000Hz = 2,
  k48000Hz = 3,
  k96000Hz = 4,
  k11025Hz = 5,
  k22050Hz = 6,
  k44100Hz = 7,
  k88200Hz = 8,
  k176400Hz = 9,
  k192000Hz = 10,
  k24000Hz = 11,
  k384000Hz = 12,
  k768000Hz = 13,
  // Must always equal the largest value ever reported:
  kAudioSampleRateMax = k768000Hz,
};

// Helper method to convert integral values to their respective enum values,
// returns false for unexpected sample rates.
MEDIA_EXPORT bool ToAudioSampleRate(int sample_rate, AudioSampleRate* asr);

}  // namespace media

#endif  // MEDIA_BASE_SAMPLE_RATES_H_
