// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_COMMON_OPENSCREEN_CONVERSION_HELPERS_H_
#define MEDIA_CAST_COMMON_OPENSCREEN_CONVERSION_HELPERS_H_

#include "media/base/audio_codecs.h"
#include "media/base/video_codecs.h"
#include "media/cast/common/sender_encoded_frame.h"
#include "media/mojo/mojom/remoting_common.mojom.h"
#include "net/base/ip_address.h"
#include "third_party/openscreen/src/cast/streaming/capture_configs.h"
#include "third_party/openscreen/src/cast/streaming/public/constants.h"
#include "third_party/openscreen/src/cast/streaming/public/offer_messages.h"
#include "third_party/openscreen/src/cast/streaming/public/sender.h"
#include "third_party/openscreen/src/cast/streaming/remoting_capabilities.h"
#include "third_party/openscreen/src/cast/streaming/rtp_time.h"
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

openscreen::cast::AudioCodec ToOpenscreenAudioCodec(media::AudioCodec codec);
openscreen::cast::VideoCodec ToOpenscreenVideoCodec(media::VideoCodec codec);
media::AudioCodec ToAudioCodec(openscreen::cast::AudioCodec codec);
media::VideoCodec ToVideoCodec(openscreen::cast::VideoCodec codec);

openscreen::IPAddress ToOpenscreenIPAddress(const net::IPAddress& address);

openscreen::cast::AudioCaptureConfig ToOpenscreenAudioConfig(
    const FrameSenderConfig& config);
openscreen::cast::VideoCaptureConfig ToOpenscreenVideoConfig(
    const FrameSenderConfig& config);

media::mojom::RemotingSinkAudioCapability ToRemotingAudioCapability(
    openscreen::cast::AudioCapability capability);
media::mojom::RemotingSinkVideoCapability ToRemotingVideoCapability(
    openscreen::cast::VideoCapability capability);

// Convert the sink capabilities to media::mojom::RemotingSinkMetadata.
media::mojom::RemotingSinkMetadata ToRemotingSinkMetadata(
    const openscreen::cast::RemotingCapabilities& capabilities,
    std::string_view friendly_name);

}  // namespace media::cast

#endif  // MEDIA_CAST_COMMON_OPENSCREEN_CONVERSION_HELPERS_H_
