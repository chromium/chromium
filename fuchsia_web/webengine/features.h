// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_FEATURES_H_
#define FUCHSIA_WEB_WEBENGINE_FEATURES_H_

#include "base/feature_list.h"

namespace features {

BASE_FEATURE(kHandleMemoryPressureInRenderer,
             "HandleMemoryPressureInRenderer",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the use of video codecs that cannot be hardware-accelerated.
// When disabled, software video decoders are still available in case they are
// needed as a fallback due to a hardware decoder failure. Does not affect
// WebRTC; see media::kExposeSwDecodersToWebRTC and
// media::kUseDecoderStreamForWebRTC.
BASE_FEATURE(kEnableSoftwareOnlyVideoCodecs,
             "SoftwareOnlyVideoCodecs",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables keyboard input handling via the fuchsia.ui.input3.Keyboard interface.
BASE_FEATURE(kKeyboardInput,
             "KeyboardInput",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables use of virtual keyboards via the
// fuchsia.input.virtualkeyboard.Controller interface.
BASE_FEATURE(kVirtualKeyboard,
             "VirtualKeyboard",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables origin trials support.
BASE_FEATURE(kOriginTrials, "OriginTrials", base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features

#endif  // FUCHSIA_WEB_WEBENGINE_FEATURES_H_
