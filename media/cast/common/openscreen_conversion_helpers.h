// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_COMMON_OPENSCREEN_CONVERSION_HELPERS_H_
#define MEDIA_CAST_COMMON_OPENSCREEN_CONVERSION_HELPERS_H_

#include "media/cast/cast_config.h"
#include "media/cast/common/sender_encoded_frame.h"
#include "net/base/ip_address.h"
#include "third_party/openscreen/src/cast/streaming/capture_configs.h"
#include "third_party/openscreen/src/cast/streaming/constants.h"
#include "third_party/openscreen/src/cast/streaming/offer_messages.h"
#include "third_party/openscreen/src/cast/streaming/rtp_time.h"
#include "third_party/openscreen/src/cast/streaming/sender.h"
#include "third_party/openscreen/src/cast/streaming/sender_message.h"
#include "third_party/openscreen/src/platform/api/time.h"
#include "ui/gfx/geometry/size.h"

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

const openscreen::cast::EncodedFrame ToOpenscreenEncodedFrame(
    const SenderEncodedFrame& encoded_frame);

openscreen::cast::AudioCodec ToOpenscreenAudioCodec(media::cast::Codec codec);
openscreen::cast::VideoCodec ToOpenscreenVideoCodec(media::cast::Codec codec);
media::cast::Codec ToCodec(openscreen::cast::AudioCodec codec);
media::cast::Codec ToCodec(openscreen::cast::VideoCodec codec);

openscreen::IPAddress ToOpenscreenIPAddress(const net::IPAddress& address);

openscreen::cast::AudioCaptureConfig ToOpenscreenAudioConfig(
    const FrameSenderConfig& config);
openscreen::cast::VideoCaptureConfig ToOpenscreenVideoConfig(
    const FrameSenderConfig& config);

}  // namespace media::cast

#endif  // MEDIA_CAST_COMMON_OPENSCREEN_CONVERSION_HELPERS_H_
