// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_WEBRTC_HELPERS_H_
#define MEDIA_WEBRTC_HELPERS_H_

#include "base/component_export.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "third_party/webrtc/modules/audio_processing/include/audio_processing.h"

namespace media {

COMPONENT_EXPORT(MEDIA_WEBRTC)
webrtc::StreamConfig CreateStreamConfig(const AudioParameters& parameters);

// Tests whether the audio bus data can be treated as upmixed mono audio:
// Returns true if there is at most one channel or if each sample is identical
// in the first two channels.
COMPONENT_EXPORT(MEDIA_WEBRTC)
bool LeftAndRightChannelsAreSymmetric(const AudioBus& audio);

}  // namespace media

#endif  // MEDIA_WEBRTC_WEBRTC_HELPERS_H_
