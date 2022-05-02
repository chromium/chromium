// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_features.h"

#include "base/feature_list.h"
#include "build/build_config.h"

namespace features {

// When the audio service in a separate process, kill it when a hang is
// detected. It will be restarted when needed.
const base::Feature kAudioServiceOutOfProcessKillAtHang{
  "AudioServiceOutOfProcessKillAtHang",
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// If enabled, base::DumpWithoutCrashing is called whenever an audio service
// hang is detected.
const base::Feature kDumpOnAudioServiceHang{"DumpOnAudioServiceHang",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

#if BUILDFLAG(IS_ANDROID)
// Enables loading and using AAudio instead of OpenSLES on compatible devices,
// for audio output streams.
const base::Feature kUseAAudioDriver{"UseAAudioDriver",
                                     base::FEATURE_ENABLED_BY_DEFAULT};
#endif

#if BUILDFLAG(IS_CHROMEOS)
const base::Feature kCrOSSystemAEC{"CrOSSystemAECWithBoardTuningsAllowed",
                                   base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kCrOSSystemAECDeactivatedGroups{
    "CrOSSystemAECDeactivatedGroups", base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kCrOSEnforceSystemAecNsAgc{
    "CrOSEnforceSystemAecNsAgc", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kCrOSEnforceSystemAecNs{"CrOSEnforceSystemAecNs",
                                            base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kCrOSEnforceSystemAecAgc{"CrOSEnforceSystemAecAgc",
                                             base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kCrOSEnforceSystemAec{"CrOSEnforceSystemAec",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kCrOSDspBasedAecDeactivatedGroups{
    "CrOSDspBasedAecDeactivatedGroups", base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kCrOSDspBasedNsDeactivatedGroups{
    "CrOSDspBasedNsDeactivatedGroups", base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kCrOSDspBasedAgcDeactivatedGroups{
    "CrOSDspBasedAgcDeactivatedGroups", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kCrOSDspBasedAecAllowed{"CrOSDspBasedAecAllowed",
                                            base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kCrOSDspBasedNsAllowed{"CrOSDspBasedNsAllowed",
                                           base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kCrOSDspBasedAgcAllowed{"CrOSDspBasedAgcAllowed",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

#endif

#if BUILDFLAG(IS_WIN)
const base::Feature kAllowIAudioClient3{"AllowIAudioClient3",
                                        base::FEATURE_ENABLED_BY_DEFAULT};
#endif
}  // namespace features
