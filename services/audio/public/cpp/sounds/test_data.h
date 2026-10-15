// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_PUBLIC_CPP_SOUNDS_TEST_DATA_H_
#define SERVICES_AUDIO_PUBLIC_CPP_SOUNDS_TEST_DATA_H_

#include <stddef.h>

namespace audio {

inline constexpr int kTestAudioKey = 1000;

inline constexpr char kTestAudioData[] =
    "RIFF\x28\x00\x00\x00WAVEfmt \x10\x00\x00\x00"
    "\x01\x00\x02\x00\x80\xbb\x00\x00\x00\x77\x01\x00\x02\x00\x10\x00"
    "data\x04\x00\x00\x00\x01\x00\x01\x00";
inline constexpr size_t kTestAudioDataSize = sizeof(kTestAudioData) - 1;

}  // namespace audio

#endif  // SERVICES_AUDIO_PUBLIC_CPP_SOUNDS_TEST_DATA_H_
