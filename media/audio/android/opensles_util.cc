// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/android/opensles_util.h"

#include "base/logging.h"

namespace media {

#define SL_ANDROID_SPEAKER_QUAD                                            \
  (SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT | SL_SPEAKER_BACK_LEFT | \
   SL_SPEAKER_BACK_RIGHT)
#define SL_ANDROID_SPEAKER_5DOT1                                              \
  (SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT | SL_SPEAKER_FRONT_CENTER | \
   SL_SPEAKER_LOW_FREQUENCY | SL_SPEAKER_BACK_LEFT | SL_SPEAKER_BACK_RIGHT)
#define SL_ANDROID_SPEAKER_7DOT1 \
  (SL_ANDROID_SPEAKER_5DOT1 | SL_SPEAKER_SIDE_LEFT | SL_SPEAKER_SIDE_RIGHT)

// Ported from:
// https://android.googlesource.com/platform/frameworks/wilhelm/+/refs/heads/master/src/android/channels.h
// https://android.googlesource.com/platform/frameworks/wilhelm/+/refs/heads/master/src/android/channels.c
SLuint32 ChannelCountToSLESChannelMask(int channel_count) {
  if (channel_count > 2) {
    LOG(WARNING) << "Guessing channel layout for " << channel_count
                 << " channels; speaker order may be incorrect.";
  }

  switch (channel_count) {
    case 1:
      return SL_SPEAKER_FRONT_LEFT;
    case 2:
      return SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
    case 3:
      return SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT |
             SL_SPEAKER_FRONT_CENTER;
    case 4:
      return SL_ANDROID_SPEAKER_QUAD;
    case 5:
      return SL_ANDROID_SPEAKER_QUAD | SL_SPEAKER_FRONT_CENTER;
    case 6:
      return SL_ANDROID_SPEAKER_5DOT1;
    case 7:
      return SL_ANDROID_SPEAKER_5DOT1 | SL_SPEAKER_BACK_CENTER;
    case 8:
      return SL_ANDROID_SPEAKER_7DOT1;
  }

  return 0;
}

}  // namespace media
