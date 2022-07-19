// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_COMMON_OPENSCREEN_CONVERSION_HELPERS_H_
#define MEDIA_CAST_COMMON_OPENSCREEN_CONVERSION_HELPERS_H_

#include "media/cast/common/sender_encoded_frame.h"
#include "third_party/openscreen/src/cast/streaming/rtp_time.h"
#include "third_party/openscreen/src/cast/streaming/sender.h"
#include "third_party/openscreen/src/platform/api/time.h"

// Conversion methods for common Open Screen media cast types. Note that many
// of these types are nearly identical in implementation.
namespace media::cast {

openscreen::Clock::time_point ToOpenscreenTimePoint(base::TimeTicks ticks);

openscreen::cast::RtpTimeTicks ToRtpTimeTicks(base::TimeDelta delta,
                                              int timebase);

openscreen::cast::RtpTimeDelta ToRtpTimeDelta(base::TimeDelta delta,
                                              int timebase);

base::TimeDelta ToTimeDelta(openscreen::cast::RtpTimeDelta rtp_delta,
                            int timebase);
base::TimeDelta ToTimeDelta(openscreen::cast::RtpTimeTicks rtp_ticks,
                            int timebase);
base::TimeDelta ToTimeDelta(openscreen::Clock::duration tp);

// TODO(https://crbug.com/1343116): as part of libcast implementation, we
// should remove media::cast::EncodedFrame::Dependency in favor of using
// the openscreen type throughout.
openscreen::cast::EncodedFrame::Dependency ToOpenscreenDependency(
    media::cast::EncodedFrame::Dependency dependency);

const openscreen::cast::EncodedFrame ToOpenscreenEncodedFrame(
    const SenderEncodedFrame& encoded_frame);

}  // namespace media::cast

#endif  // MEDIA_CAST_COMMON_OPENSCREEN_CONVERSION_HELPERS_H_
