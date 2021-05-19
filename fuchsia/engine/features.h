// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_FEATURES_H_
#define FUCHSIA_ENGINE_FEATURES_H_

#include "base/feature_list.h"

namespace features {

constexpr base::Feature kHandleMemoryPressureInRenderer{
    "HandleMemoryPressureInRenderer", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the use of video codecs that cannot be hardware-accelerated.
// When disabled, software video decoders are still available in case they are
// needed as a fallback due to a hardware decoder failure. Does not affect
// WebRTC; see media::kExposeSwDecodersToWebRTC and
// media::kUseDecoderStreamForWebRTC.
constexpr base::Feature kEnableSoftwareOnlyVideoCodecs{
    "SoftwareOnlyVideoCodecs", base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace features

#endif  // FUCHSIA_ENGINE_FEATURES_H_
