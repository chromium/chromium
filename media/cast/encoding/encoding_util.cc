// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/encoding/encoding_util.h"

#include "media/base/media_switches.h"

namespace media::cast {

uint8_t GetEncoderDropFrameThreshold() {
  // Use a smaller frame drop threshold than WebRTC (30) because a user expects
  // frames are not dropped so much in cast mirroring.
  return base::FeatureList::IsEnabled(kCastVideoEncoderFrameDrop) ? 10 : 0;
}

}  // namespace media::cast
