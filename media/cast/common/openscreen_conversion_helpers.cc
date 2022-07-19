// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/common/openscreen_conversion_helpers.h"

namespace media::cast {

openscreen::Clock::time_point ToOpenscreenTimePoint(base::TimeTicks ticks) {
  static_assert(sizeof(openscreen::Clock::time_point::rep) >=
                sizeof(base::TimeTicks::Max()));
  return openscreen::Clock::time_point(
      std::chrono::microseconds(ticks.since_origin().InMicroseconds()));
}

// Returns the tick count in the given timebase nearest to the base::TimeDelta.
int64_t TimeDeltaToTicks(base::TimeDelta delta, int timebase) {
  DCHECK_GT(timebase, 0);
  const double ticks = delta.InSecondsF() * timebase + 0.5 /* rounding */;
  return base::checked_cast<int64_t>(ticks);
}

openscreen::cast::RtpTimeTicks ToRtpTimeTicks(base::TimeDelta delta,
                                              int timebase) {
  return openscreen::cast::RtpTimeTicks(TimeDeltaToTicks(delta, timebase));
}

openscreen::cast::RtpTimeDelta ToRtpTimeDelta(base::TimeDelta delta,
                                              int timebase) {
  return openscreen::cast::RtpTimeDelta::FromTicks(
      TimeDeltaToTicks(delta, timebase));
}

base::TimeDelta ToTimeDelta(openscreen::cast::RtpTimeDelta rtp_delta,
                            int timebase) {
  DCHECK_GT(timebase, 0);
  return base::Microseconds(
      rtp_delta.ToDuration<std::chrono::microseconds>(timebase).count());
}

base::TimeDelta ToTimeDelta(openscreen::cast::RtpTimeTicks rtp_ticks,
                            int timebase) {
  DCHECK_GT(timebase, 0);
  return ToTimeDelta(rtp_ticks - openscreen::cast::RtpTimeTicks{}, timebase);
}

base::TimeDelta ToTimeDelta(openscreen::Clock::duration tp) {
  return base::Microseconds(
      std::chrono::duration_cast<std::chrono::microseconds>(tp).count());
}

openscreen::cast::EncodedFrame::Dependency ToOpenscreenDependency(
    media::cast::EncodedFrame::Dependency dependency) {
  switch (dependency) {
    case media::cast::EncodedFrame::Dependency::UNKNOWN_DEPENDENCY:
      return openscreen::cast::EncodedFrame::Dependency::UNKNOWN_DEPENDENCY;
    case media::cast::EncodedFrame::Dependency::DEPENDENT:
      return openscreen::cast::EncodedFrame::Dependency::DEPENDS_ON_ANOTHER;
    case media::cast::EncodedFrame::Dependency::INDEPENDENT:
      return openscreen::cast::EncodedFrame::Dependency::
          INDEPENDENTLY_DECODABLE;
    case media::cast::EncodedFrame::Dependency::KEY:
      return openscreen::cast::EncodedFrame::Dependency::KEY_FRAME;
    default:
      NOTREACHED();
      break;
  }
}
const openscreen::cast::EncodedFrame ToOpenscreenEncodedFrame(
    const SenderEncodedFrame& encoded_frame) {
  return openscreen::cast::EncodedFrame(
      ToOpenscreenDependency(encoded_frame.dependency), encoded_frame.frame_id,
      encoded_frame.referenced_frame_id, encoded_frame.rtp_timestamp,
      ToOpenscreenTimePoint(encoded_frame.reference_time),
      std::chrono::milliseconds(encoded_frame.new_playout_delay_ms),
      // We return a const EncodedFrame, so this is safe even though weird.
      absl::Span<uint8_t>(const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(
                              encoded_frame.data.data())),
                          encoded_frame.data.size()));
}

}  // namespace media::cast
