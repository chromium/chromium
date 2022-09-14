// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/sample_rates.h"

#include "base/check.h"

namespace media {

bool ToAudioSampleRate(int sample_rate, AudioSampleRate* asr) {
  DCHECK(asr);
  switch (sample_rate) {
    case 8000:
      *asr = k8000Hz;
      return true;
    case 16000:
      *asr = k16000Hz;
      return true;
    case 24000:
      *asr = k24000Hz;
      return true;
    case 32000:
      *asr = k32000Hz;
      return true;
    case 48000:
      *asr = k48000Hz;
      return true;
    case 96000:
      *asr = k96000Hz;
      return true;
    case 11025:
      *asr = k11025Hz;
      return true;
    case 22050:
      *asr = k22050Hz;
      return true;
    case 44100:
      *asr = k44100Hz;
      return true;
    case 88200:
      *asr = k88200Hz;
      return true;
    case 176400:
      *asr = k176400Hz;
      return true;
    case 192000:
      *asr = k192000Hz;
      return true;
    case 384000:
      *asr = k384000Hz;
      return true;
    case 768000:
      *asr = k768000Hz;
      return true;
  }
  return false;
}

}  // namespace media
