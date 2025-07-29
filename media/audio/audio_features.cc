// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_features.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "media/media_buildflags.h"

namespace features {

#if BUILDFLAG(IS_WIN)
// Enables application audio capture for getDisplayMedia (gDM) window capture in
// Windows.
BASE_FEATURE(kApplicationAudioCaptureWin,
             "ApplicationAudioCaptureWin",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_ANDROID)
// Enables loading and using AAudio instead of OpenSLES on compatible devices,
// for audio output streams. This feature is disabled on ATV HDMI dongle devices
// as OpenSLES provides more accurate output latency on those devices.
//
// TODO(crbug.com/401365323): Remove this feature in the future.
BASE_FEATURE(kUseAAudioDriver,
             "UseAAudioDriver",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables loading and using AAudio instead of OpenSLES on compatible devices,
// for audio input streams.
BASE_FEATURE(kUseAAudioInput,
             "UseAAudioInput",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables selection of audio devices for each individual AAudio stream instead
// of using communication streams and managing the system-wide communication
// route. This is not fully reliable on all Android devices.
//
// Requires `UseAAudioDriver`, `UseAAudioInput`, and an Android API level >=
// `AAUDIO_MIN_API`, otherwise it will have no effect.
BASE_FEATURE(kAAudioPerStreamDeviceSelection,
             "AAudioPerStreamDeviceSelection",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// This feature flag controls whether the WebAudio destination resampler is
// bypassed. When enabled, if the WebAudio context's sample rate differs from
// the hardware's sample rate, the resampling step that normally occurs within
// the WebAudio destination node is skipped. This allows the AudioService to
// handle any necessary resampling, potentially reducing latency and overhead.
BASE_FEATURE(kWebAudioRemoveAudioDestinationResampler,
             "WebAudioRemoveAudioDestinationResampler",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_DISABLED_BY_DEFAULT);
#else
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace features

namespace media {

bool IsApplicationAudioCaptureSupported() {
#if BUILDFLAG(IS_WIN)
  return base::FeatureList::IsEnabled(features::kApplicationAudioCaptureWin);
#else
  return false;
#endif  // BUILDFLAG(IS_WIN)
}

}  // namespace media
