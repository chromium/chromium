// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_FEATURES_H_
#define MEDIA_AUDIO_AUDIO_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "media/base/media_export.h"

namespace features {

#if defined(OS_CHROMEOS)
MEDIA_EXPORT extern const base::Feature kEnumerateAudioDevices;
MEDIA_EXPORT extern const base::Feature kCrOSSystemAEC;
#endif

#if defined(OS_WIN)
MEDIA_EXPORT extern const base::Feature kIncreaseInputAudioBufferSize;
#endif

}  // namespace features

#endif  // MEDIA_AUDIO_AUDIO_FEATURES_H_
