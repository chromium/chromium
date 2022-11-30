// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/media_session/public/cpp/features.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace media_session {
namespace features {

// Enables the Media Session service including audio focus tracking. This allows
// clients to consume the Media Session Mojo APIs but should not have any
// changes to behavior. It is enabled by default on all platforms except Android.
BASE_FEATURE(kMediaSessionService,
             "MediaSessionService",
#if !BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// Enables Audio Focus enforcement which means that only one media session can
// have audio focus at any one time.
BASE_FEATURE(kAudioFocusEnforcement,
             "AudioFocusEnforcement",
#if BUILDFLAG(IS_CHROMEOS_ASH)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// Enables Audio Focus grouping which means that multiple media sessions can
// share audio focus at the same time provided that they have the same group id.
BASE_FEATURE(kAudioFocusSessionGrouping,
             "AudioFocusSessionGrouping",
#if BUILDFLAG(IS_CHROMEOS_ASH)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

}  // namespace features
}  // namespace media_session
