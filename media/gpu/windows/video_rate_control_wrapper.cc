// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(tsepez): work around what appears to be a compiler issue.
#pragma allow_unsafe_libc_calls
#endif

#include "media/gpu/windows/video_rate_control_wrapper.h"

namespace media {

VideoRateControlWrapper::RateControlConfig::RateControlConfig() = default;
VideoRateControlWrapper::RateControlConfig::~RateControlConfig() = default;
VideoRateControlWrapper::RateControlConfig::RateControlConfig(
    const RateControlConfig&) = default;
VideoRateControlWrapper::RateControlConfig&
VideoRateControlWrapper::RateControlConfig::operator=(
    const RateControlConfig&) = default;

}  // namespace media
