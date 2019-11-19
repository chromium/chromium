// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_FEATURES_H_
#define MEDIA_AUDIO_AUDIO_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "media/base/media_export.h"

namespace features {

MEDIA_EXPORT extern const base::Feature kAudioServiceOutOfProcessKillAtHang;
MEDIA_EXPORT extern const base::Feature kDumpOnAudioServiceHang;

#if defined(OS_ANDROID)
MEDIA_EXPORT extern const base::Feature kUseAAudioDriver;
#endif

#if defined(OS_CHROMEOS)
MEDIA_EXPORT extern const base::Feature kCrOSSystemAEC;
MEDIA_EXPORT extern const base::Feature kCrOSSystemAECDeactivatedGroups;
#endif

#if defined(OS_MACOSX) || defined(OS_CHROMEOS)
MEDIA_EXPORT extern const base::Feature kForceEnableSystemAec;
#endif

#if defined(OS_WIN)
MEDIA_EXPORT extern const base::Feature kAllowIAudioClient3;
#endif

}  // namespace features

#endif  // MEDIA_AUDIO_AUDIO_FEATURES_H_
