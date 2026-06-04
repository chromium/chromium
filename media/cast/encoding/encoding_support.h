// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_ENCODING_ENCODING_SUPPORT_H_
#define MEDIA_CAST_ENCODING_ENCODING_SUPPORT_H_

#include <vector>

#include "media/video/video_encode_accelerator.h"

namespace gfx {
class Size;
}

namespace media {

enum class VideoCodec;

// This namespace includes free functions that determine if software and
// hardware encoding is available and should be used for different codecs. Note
// that these methods also check the current configuration of the
// kCastStreaming* feature flags in //media/base/media_switches.h.
namespace cast::encoding_support {

// Returns true if software encoding is supported for this codec.
bool IsSoftwareEnabled(VideoCodec codec);

// Returns true if a hardware encoder should be used for a codec with a
// given receiver and set of VEA profiles. Some receivers have implementation
// bugs that keep the external encoder from being used even if it is supported
// by the sender.
bool IsHardwareEnabled(
    VideoCodec codec,
    const std::vector<media::VideoEncodeAccelerator::SupportedProfile>&
        profiles,
    gfx::Size requested_resolution,
    double requested_frame_rate);

// Should be called before calling DenyListHardwareCodec() to ensure that we
// don't attempt to disable the codec multiple times.
bool IsHardwareDenyListed(VideoCodec codec);

// Call to disable a hardware codec using a singleton.
void DenyListHardwareCodec(VideoCodec codec);

// Reset the global hardware codec deny list for use in testing.
void ClearHardwareCodecDenyListForTesting();

// Gets the preferred codec profile (usually some flavor of "main") for the
// given `codec`.
VideoCodecProfile ToProfile(VideoCodec codec);

// Gets the codec parameter string (based on the related VideoCodecProfile) for
// the provided `codec`, taking into account resolution and frame rate to
// determine the required level.
//
// We simplify the level calculation by only distinguishing between <= 1080p30
// and higher configurations (like 1080p60). Offering the minimum required level
// maximizes compatibility with receivers that might reject offers claiming
// higher levels than they support, even if they could handle the actual content
// (e.g., Chromecast 1 only supports up to H.264 Level 4.1, and offering Level
// 4.2 for a 1080p30 stream might cause it to be rejected).
std::string GetCodecParameterString(VideoCodec codec,
                                    gfx::Size resolution,
                                    double frame_rate);

}  // namespace cast::encoding_support
}  // namespace media

#endif  // MEDIA_CAST_ENCODING_ENCODING_SUPPORT_H_
