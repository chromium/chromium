// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_TEST_DATA_H_
#define MEDIA_AUDIO_TEST_DATA_H_

namespace media {

const char kTestAudioData[] =
    "RIFF\x28\x00\x00\x00WAVEfmt \x10\x00\x00\x00"
    "\x01\x00\x02\x00\x80\xbb\x00\x00\x00\x77\x01\x00\x02\x00\x10\x00"
    "data\x04\x00\x00\x00\x01\x00\x01\x00";
const size_t kTestAudioDataSize = std::size(kTestAudioData) - 1;

// Extensible format with 48kHz rate stereo 32 bit float samples
const char kTestFloatAudioData[] =
    "RIFF\x26\x00\x00\x00WAVEfmt \x10\x00\x00\x00"
    "\x03\x00\x02\x00\x80\xbb\x00\x00\x00\xdc\x05\x00\x08\x00\x20\x00"
    "data\x08\x00\x00\x00\x00\x01\x00\x00\x01\x00\x00\x00";
const size_t kTestFloatAudioDataSize = std::size(kTestFloatAudioData) - 1;

// Extensible format with 48kHz rate stereo 32 bit PCM samples
const char kTestExtensibleAudioData[] =
    "RIFF\x44\x00\x00\x00WAVEfmt \x28\x00\x00\x00"
    "\xfe\xff\x02\x00\x80\xbb\x00\x00\x00\x77\x01\x00\x02\x00\x20\x00"
    "\x16\x00\x20\x00\x00\x00\x00\x00"
    "\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    "data\x08\x00\x00\x00\x01\x00\x00\x00\x01\x00\x00\x00";
const size_t kTestExtensibleAudioDataSize =
    std::size(kTestExtensibleAudioData) - 1;

}  // namespace media

#endif  // MEDIA_AUDIO_TEST_DATA_H_
