// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_features.h"

#include "base/feature_list.h"
#include "build/build_config.h"

namespace features {

// If enabled, base::DumpWithoutCrashing is called whenever an audio service
// hang is detected.
BASE_FEATURE(kDumpOnAudioServiceHang,
             "DumpOnAudioServiceHang",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
// Enables loading and using AAudio instead of OpenSLES on compatible devices,
// for audio output streams.
BASE_FEATURE(kUseAAudioDriver,
             "UseAAudioDriver",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables loading and using AAudio instead of OpenSLES on compatible devices,
// for audio input streams.
BASE_FEATURE(kUseAAudioInput,
             "UseAAudioInput",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_WIN)
BASE_FEATURE(kAllowIAudioClient3,
             "AllowIAudioClient3",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif
}  // namespace features
