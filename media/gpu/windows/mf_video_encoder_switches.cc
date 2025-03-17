// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/mf_video_encoder_switches.h"

namespace media {

BASE_FEATURE(kExpandMediaFoundationEncodingResolutions,
             "ExpandMediaFoundationEncodingResolutions",
             base::FEATURE_ENABLED_BY_DEFAULT);

#ifndef ARCH_CPU_X86
// Temporal layers are reported to be supported by the Intel driver, but are
// only considered supported by MediaFoundation depending on these flags. This
// support is reported in MediaCapabilities' powerEfficient as well as deciding
// if Initialize() is allowed to succeed.
BASE_FEATURE(kMediaFoundationVP9L1T2Support,
             "MediaFoundationVP9L1T2Support",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Up to 3 temporal layers, i.e. this enables both L1T2 and L1T3.
BASE_FEATURE(kMediaFoundationVP9L1T3Support,
             "MediaFoundationVP9L1T3Support",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kMediaFoundationAV1L1T2Support,
             "MediaFoundationAV1L1T2Support",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Up to 3 temporal layers, i.e. this enables both L1T2 and L1T3.
BASE_FEATURE(kMediaFoundationAV1L1T3Support,
             "MediaFoundationAV1L1T3Support",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // !defined(ARCH_CPU_X86)

BASE_FEATURE(kMediaFoundationUseSWBRCForH264Camera,
             "MediaFoundationUseSWBRCForH264Camera",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kMediaFoundationUseSWBRCForH264Desktop,
             "MediaFoundationUseSWBRCForH264Desktop",
             base::FEATURE_DISABLED_BY_DEFAULT);

// H.264 SW Bitrate Controller works in fixed delta QP mode by default. The QP
// difference between base and enhancement layer can be controlled using a
// feature parameter.
BASE_FEATURE(kMediaFoundationSWBRCUseFixedDeltaQP,
             "MediaFoundationSWBRCUseFixedDeltaQP",
             base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<int> kMediaFoundationSWBRCFixedDeltaQPValue(
    &kMediaFoundationSWBRCUseFixedDeltaQP,
    "MediaFoundationSWBRCFixedDeltaQPValue",
    0);

#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
// For H.265 encoding at L1T1/L1T2 we may use SW bitrate controller when
// constant bitrate encoding is requested.
BASE_FEATURE(kMediaFoundationUseSWBRCForH265,
             "MediaFoundationUseSWBRCForH265",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC)

}  // namespace media
