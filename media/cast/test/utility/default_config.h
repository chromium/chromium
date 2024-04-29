// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_TEST_UTILITY_DEFAULT_CONFIG_H_
#define MEDIA_CAST_TEST_UTILITY_DEFAULT_CONFIG_H_

#include "media/cast/cast_config.h"

namespace media {
namespace cast {

// Returns a FrameSenderConfig initialized to default values. This means
// 48 kHz, 2-channel Opus-coded audio. Default values for SSRCs and payload
// type.
FrameSenderConfig GetDefaultAudioSenderConfig();

// Returns a FrameSenderConfig initialized to default values. This means
// 30 Hz VP8 coded code. Default values for SSRCs and payload type.
FrameSenderConfig GetDefaultVideoSenderConfig();

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_TEST_UTILITY_DEFAULT_CONFIG_H_
