// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_features.h"
#include "base/feature_list.h"

namespace features {

// When the audio service in a separate process, kill it when a hang is
// detected. It will be restarted when needed.
const base::Feature kAudioServiceOutOfProcessKillAtHang{
  "AudioServiceOutOfProcessKillAtHang",
#if defined(OS_WIN) || defined(OS_MACOSX)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// If enabled, base::DumpWithoutCrashing is called whenever an audio service
// hang is detected.
const base::Feature kDumpOnAudioServiceHang{"DumpOnAudioServiceHang",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(OS_ANDROID)
// Enables loading and using AAudio instead of OpenSLES on compatible devices,
// for audio output streams.
const base::Feature kUseAAudioDriver{"UseAAudioDriver",
                                     base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if defined(OS_CHROMEOS)
const base::Feature kCrOSSystemAEC{"CrOSSystemAEC",
                                   base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kCrOSSystemAECDeactivatedGroups{
    "CrOSSystemAECDeactivatedGroups", base::FEATURE_ENABLED_BY_DEFAULT};
#endif

#if defined(OS_MACOSX) || defined(OS_CHROMEOS)
const base::Feature kForceEnableSystemAec{"ForceEnableSystemAec",
                                          base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if defined(OS_WIN)
const base::Feature kAllowIAudioClient3{"AllowIAudioClient3",
                                        base::FEATURE_ENABLED_BY_DEFAULT};
#endif
}  // namespace features
