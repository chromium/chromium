// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_MF_VIDEO_ENCODER_SWITCHES_H_
#define MEDIA_GPU_WINDOWS_MF_VIDEO_ENCODER_SWITCHES_H_

#include <stdint.h>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "media/gpu/buildflags.h"
#include "media/media_buildflags.h"

namespace media {

BASE_DECLARE_FEATURE(kExpandMediaFoundationEncodingResolutions);

#ifndef ARCH_CPU_X86
BASE_DECLARE_FEATURE(kMediaFoundationVP9L1T2Support);
BASE_DECLARE_FEATURE(kMediaFoundationVP9L1T3Support);
BASE_DECLARE_FEATURE(kMediaFoundationAV1L1T2Support);
BASE_DECLARE_FEATURE(kMediaFoundationAV1L1T3Support);
#endif  // !defined(ARCH_CPU_X86)

BASE_DECLARE_FEATURE(kMediaFoundationUseSWBRCForH264Camera);
BASE_DECLARE_FEATURE(kMediaFoundationUseSWBRCForH264Desktop);

BASE_DECLARE_FEATURE(kMediaFoundationSWBRCUseFixedDeltaQP);
extern const base::FeatureParam<int> kMediaFoundationSWBRCFixedDeltaQPValue;

#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
BASE_DECLARE_FEATURE(kMediaFoundationUseSWBRCForH265);
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC)

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_MF_VIDEO_ENCODER_SWITCHES_H_
