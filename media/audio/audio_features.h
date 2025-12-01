// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_FEATURES_H_
#define MEDIA_AUDIO_AUDIO_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "media/base/media_export.h"

namespace features {

#if BUILDFLAG(IS_WIN)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kApplicationAudioCaptureWin);
#endif

#if BUILDFLAG(IS_ANDROID)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kUseAAudioDriver);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kUseAAudioInput);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kAAudioPerStreamDeviceSelection);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kAlwaysUseAudioManagerOutputFramesPerBuffer);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kAndroidAudioDeviceListener);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kAudioStereoInputStreamParameters);
#endif

#if BUILDFLAG(IS_MAC)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMacAVFoundationPlayback);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMacCatapRestartOnDeviceChange);
#endif

MEDIA_EXPORT BASE_DECLARE_FEATURE(kWebAudioRemoveAudioDestinationResampler);

}  // namespace features

namespace media {

// Returns true if application audio capture is implemented for the current OS.
MEDIA_EXPORT bool IsApplicationAudioCaptureSupported();

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_FEATURES_H_
