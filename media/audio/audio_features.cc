// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_features.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "media/media_buildflags.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

namespace {
#if BUILDFLAG(IS_MAC)
// Enables system audio loopback capture using the macOS Screen Capture Kit
// framework, regardless of the system version.
BASE_FEATURE(kMacSckSystemAudioLoopbackOverride,
             "MacSckSystemAudioLoopbackOverride",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif
}  // namespace

namespace features {

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

#if BUILDFLAG(IS_MAC)
// Enables system audio loopback capture using the macOS CoreAudio tap API.
BASE_FEATURE(kMacCatapSystemAudioLoopbackCapture,
             "MacCatapSystemAudioLoopbackCapture",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

}  // namespace features

namespace media {
#if BUILDFLAG(IS_MAC)
bool IsMacCatapSystemAudioLoopbackCaptureEnabled() {
  return (base::mac::MacOSVersion() >= 14'02'00 &&
          base::FeatureList::IsEnabled(
              features::kMacCatapSystemAudioLoopbackCapture));
}

bool IsMacSckSystemAudioLoopbackCaptureEnabled() {
  // Only supported on macOS 13.0+.
  // Disabled on macOS 15.0 due to problems with permission prompt.
  // The override feature is useful for testing on unsupported versions.
  return (base::mac::MacOSVersion() >= 13'00'00 &&
          base::mac::MacOSVersion() < 15'00'00) ||
         base::FeatureList::IsEnabled(kMacSckSystemAudioLoopbackOverride);
}
#endif

bool IsSystemLoopbackCaptureSupported() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(USE_CRAS)
  return true;
#elif BUILDFLAG(IS_MAC)
  // For system audio loopback to be enabled in getDisplayMedia(), the feature
  // kMacLoopbackAudioForScreenShare must also be enabled.
  // TODO(crbug.com/365602111): Implement SCContentPicker compatible capture
  // for MacOS 15.
  return IsMacSckSystemAudioLoopbackCaptureEnabled() ||
         IsMacCatapSystemAudioLoopbackCaptureEnabled();
#elif BUILDFLAG(IS_LINUX) && defined(USE_PULSEAUDIO)
  return true;
#else
  return false;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(USE_CRAS)
}

}  // namespace media
