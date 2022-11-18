// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_ENCODING_ENCODING_SUPPORT_H_
#define MEDIA_CAST_ENCODING_ENCODING_SUPPORT_H_

#include <vector>

#include "media/cast/cast_config.h"
#include "media/video/video_encode_accelerator.h"

// This namespace includes free functions that determine if software and
// hardware encoding is available and should be used for different codecs. Note
// that these methods also check the current configuration of the
// kCastStreaming* feature flags in //media/base/media_switches.h.
namespace media::cast::encoding_support {

// Returns true if software encoding is supported for this codec.
bool IsSoftwareEnabled(Codec codec);

// Returns true if a hardware encoder should be used for a codec with a
// given receiver and set of VEA profiles. Some receivers have implementation
// bugs that keep the external encoder from being used even if it is supported
// by the sender.
bool IsHardwareEnabled(
    Codec codec,
    const std::vector<media::VideoEncodeAccelerator::SupportedProfile>&
        profiles);

}  // namespace media::cast::encoding_support

#endif  // MEDIA_CAST_ENCODING_ENCODING_SUPPORT_H_
