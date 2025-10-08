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
BASE_FEATURE(kApplicationAudioCaptureWin, base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_ANDROID)
// Enables loading and using AAudio instead of OpenSLES on compatible devices,
// for audio output streams. This feature is disabled on ATV HDMI dongle devices
// as OpenSLES provides more accurate output latency on those devices.
//
// TODO(crbug.com/401365323): Remove this feature in the future.
BASE_FEATURE(kUseAAudioDriver, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables loading and using AAudio instead of OpenSLES on compatible devices,
// for audio input streams.
BASE_FEATURE(kUseAAudioInput, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables selection of audio devices for each individual AAudio stream instead
// of using communication streams and managing the system-wide communication
// route. This is not fully reliable on all Android devices.
//
// Requires `UseAAudioDriver` and `UseAAudioInput`, otherwise it will have no
// effect.
BASE_FEATURE(kAAudioPerStreamDeviceSelection,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Use buffer size from AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER for
// optimal output frame size.
BASE_FEATURE(kAlwaysUseAudioManagerOutputFramesPerBuffer,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the AudioDeviceListener, which listens for changes to the list of
// audio devices exposed by the OS.
BASE_FEATURE(kAndroidAudioDeviceListener, base::FEATURE_DISABLED_BY_DEFAULT);

// Use stereo channel layout for input stream parameters.
// TODO(crbug.com/440210010): Remove when the experiment is done.
BASE_FEATURE(kAudioStereoInputStreamParameters,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// This feature flag controls whether the WebAudio destination resampler is
// bypassed. When enabled, if the WebAudio context's sample rate differs from
// the hardware's sample rate, the resampling step that normally occurs within
// the WebAudio destination node is skipped. This allows the AudioService to
// handle any necessary resampling, potentially reducing latency and overhead.
BASE_FEATURE(kWebAudioRemoveAudioDestinationResampler,
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_DISABLED_BY_DEFAULT);
#else
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_MAC)
// Enabling this feature will allow AudioManagerMac to generate AVFoundation
// AudioOutputStreams instead of AUHALStreams in cases of multichannel audio.
// MacOS will then "Spatialize" the audio for users on compatible Airpods. The
// end result will give users the option to change modes on their Airpods (Off,
// Fixed, Head Tracking).
BASE_FEATURE(kMacAVFoundationPlayback, base::FEATURE_DISABLED_BY_DEFAULT);

// If this feature is enabled, and CATap is capturing the default output device,
// the CATap implementation will handle default output device changes by
// restarting the system audio capture. The changes we listen for are if
// default output device is changed to another device, or if the sample rate of
// the default output device is changed. If the feature is disabled, CATap will
// keep capturing the same device when default output device is changed, and
// will report an error if the sample rate is changed.
BASE_FEATURE(kMacCatapRestartOnDeviceChange, base::FEATURE_ENABLED_BY_DEFAULT);
#endif

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
