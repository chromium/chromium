// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines features for media/webrtc.

#ifndef MEDIA_WEBRTC_WEBRTC_FEATURES_H_
#define MEDIA_WEBRTC_WEBRTC_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "third_party/webrtc/modules/audio_processing/include/audio_processing.h"

namespace features {

COMPONENT_EXPORT(MEDIA_WEBRTC)
BASE_DECLARE_FEATURE(kWebRtcAllowWgcScreenCapturer);

COMPONENT_EXPORT(MEDIA_WEBRTC)
BASE_DECLARE_FEATURE(kWebRtcAllowWgcWindowCapturer);

COMPONENT_EXPORT(MEDIA_WEBRTC)
BASE_DECLARE_FEATURE(kWebRtcAllowDxgiGdiZeroHz);

COMPONENT_EXPORT(MEDIA_WEBRTC)
BASE_DECLARE_FEATURE(kWebRtcAllowWgcScreenZeroHz);

COMPONENT_EXPORT(MEDIA_WEBRTC)
BASE_DECLARE_FEATURE(kWebRtcAllowWgcWindowZeroHz);

COMPONENT_EXPORT(MEDIA_WEBRTC)
BASE_DECLARE_FEATURE(kWebRtcAllowInputVolumeAdjustment);

COMPONENT_EXPORT(MEDIA_WEBRTC)
BASE_DECLARE_FEATURE(kWebRtcApmDownmixCaptureAudioMethod);

COMPONENT_EXPORT(MEDIA_WEBRTC)
extern const base::FeatureParam<
    ::webrtc::AudioProcessing::Config::Pipeline::DownmixMethod>
    kWebRtcApmDownmixMethodParam;

COMPONENT_EXPORT(MEDIA_WEBRTC)
BASE_DECLARE_FEATURE(kWebRtcApmTellsIfPlayoutReferenceIsNeeded);

COMPONENT_EXPORT(MEDIA_WEBRTC)
BASE_DECLARE_FEATURE(kWebRtcAllowH265Send);

COMPONENT_EXPORT(MEDIA_WEBRTC)
BASE_DECLARE_FEATURE(kWebRtcAllowH265Receive);

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(MEDIA_WEBRTC)
BASE_DECLARE_FEATURE(kWebRtcApm48kHzSampleRateOnAndroidKillSwitch);
#endif

}  // namespace features

#endif  // MEDIA_WEBRTC_WEBRTC_FEATURES_H_
