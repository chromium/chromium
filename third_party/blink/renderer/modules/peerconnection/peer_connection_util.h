// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_PEER_CONNECTION_UTIL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_PEER_CONNECTION_UTIL_H_

#include <stdint.h>

#include "base/time/time.h"
#include "third_party/blink/renderer/core/dom/dom_high_res_time_stamp.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

// This structure represents a capture time as a timestamp together with the
// type of clock from where there timestamp comes.
// Different sources of frames use different clocks (e.g., monotonic pausable
// vs. adjustable wall clock), with different epochs (e.g., Unix, NTP,
// system startup).
struct CaptureTimeInfo {
  enum class ClockType {
    // The timestamp uses a monotonic base::TimeTicks clock and is relative to
    // the same platform-dependent epoch as base::TimeTicks::Now().
    kTimeTicks,

    // The timestamp uses a real-world clock and is relative to the NTP epoch
    // (1900-01-01). Commonly used by incoming frames using the the
    // abs-capture-time RTP Header extension.
    kNtpRealClock
  };

  base::TimeDelta capture_time;
  ClockType clock_type;
};

// Returns a DOMHighResTimeStamp relative to Performance.timeOrigin.
MODULES_EXPORT DOMHighResTimeStamp RTCTimeStampFromTimeTicks(ExecutionContext*,
                                                             base::TimeTicks);

// Returns a DOMHighResTimeStamp relative to Performance.timeOrigin.
MODULES_EXPORT DOMHighResTimeStamp
RTCEncodedFrameTimestampFromCaptureTimeInfo(ExecutionContext*, CaptureTimeInfo);

// Converts a DOMHighResTimeStamp relative to Performance.timeOrigin to a
// base::TimeDelta representing the time elapsed since the epoch associated
// with the given clock type.
MODULES_EXPORT base::TimeDelta RTCEncodedFrameTimestampToCaptureTime(
    ExecutionContext*,
    DOMHighResTimeStamp,
    CaptureTimeInfo::ClockType);

// Returns a DOMHighResTimeStamp equivalent to the given delta.
MODULES_EXPORT DOMHighResTimeStamp
CalculateRTCEncodedFrameTimeDelta(ExecutionContext*, base::TimeDelta);

// These functions convert between an audio level in -dBov and a linear double
// value in the [0.0-1.0] range. The dBov audio level is in the [0,127] range
// as defined in RFC 6464.
MODULES_EXPORT double ToLinearAudioLevel(uint8_t audio_level_dbov);
MODULES_EXPORT uint8_t FromLinearAudioLevel(double linear_audio_level);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_PEER_CONNECTION_UTIL_H_
