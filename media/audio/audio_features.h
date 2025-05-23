// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_FEATURES_H_
#define MEDIA_AUDIO_AUDIO_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "media/base/media_export.h"

namespace features {

#if BUILDFLAG(IS_ANDROID)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kUseAAudioDriver);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kUseAAudioInput);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kAAudioPerStreamDeviceSelection);
#endif

#if BUILDFLAG(IS_MAC)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMacCatapSystemAudioLoopbackCapture);
#endif

}  // namespace features

namespace media {

#if BUILDFLAG(IS_MAC)
MEDIA_EXPORT bool IsMacCatapSystemAudioLoopbackCaptureEnabled();
#endif

// Returns true if system audio loopback capture is implemented for the current
// OS.
MEDIA_EXPORT bool IsSystemLoopbackCaptureSupported();

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_FEATURES_H_
