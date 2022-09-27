// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webrtc/webrtc_features.h"

#include "base/command_line.h"
#include "build/build_config.h"

namespace features {
namespace {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
constexpr base::FeatureState kWebRtcHybridAgcState =
    base::FEATURE_ENABLED_BY_DEFAULT;
#else
constexpr base::FeatureState kWebRtcHybridAgcState =
    base::FEATURE_DISABLED_BY_DEFAULT;
#endif
}  // namespace

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
constexpr base::FeatureState kWebRtcAnalogAgcClippingControlState =
    base::FEATURE_ENABLED_BY_DEFAULT;
#else
constexpr base::FeatureState kWebRtcAnalogAgcClippingControlState =
    base::FEATURE_DISABLED_BY_DEFAULT;
#endif

// When enabled we will tell WebRTC that we want to use the
// Windows.Graphics.Capture API based DesktopCapturer, if it is available.
BASE_FEATURE(kWebRtcAllowWgcDesktopCapturer,
             "AllowWgcDesktopCapturer",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Kill-switch allowing deactivation of the support for 48 kHz internal
// processing in the WebRTC audio processing module when running on an ARM
// platform.
BASE_FEATURE(kWebRtcAllow48kHzProcessingOnArm,
             "WebRtcAllow48kHzProcessingOnArm",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the WebRTC Hybrid AGC configuration - i.e., AGC1 analog and AGC2
// digital (see http://crbug.com/1231085).
BASE_FEATURE(kWebRtcHybridAgc, "WebRtcHybridAgc", kWebRtcHybridAgcState);

// Enables and configures the clipping control in the WebRTC analog AGC.
BASE_FEATURE(kWebRtcAnalogAgcClippingControl,
             "WebRtcAnalogAgcClippingControl",
             kWebRtcAnalogAgcClippingControlState);

// Enables the override for the default minimum starting volume of the Automatic
// Gain Control algorithm in WebRTC.
BASE_FEATURE(kWebRtcAnalogAgcStartupMinVolume,
             "WebRtcAnalogAgcStartupMinVolume",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features
