// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_CONSTANTS_H_
#define MEDIA_CAST_CONSTANTS_H_

////////////////////////////////////////////////////////////////////////////////
// NOTE: This file should only contain constants that are reasonably globally
// used (i.e., by many modules, and in all or nearly all subdirs).  Do NOT add
// non-POD constants, functions, interfaces, or any logic to this module.
////////////////////////////////////////////////////////////////////////////////

#include <stdint.h>

#include "base/time/time.h"

namespace media {
namespace cast {

// Integer constants set either by the Cast Streaming Protocol Spec or due to
// design limitations.
enum Specifications {

  // This is an important system-wide constant.  This limits how much history
  // the implementation must retain in order to process the acknowledgements of
  // past frames.
  //
  // This value is carefully chosen such that it fits in the 8-bits range for
  // frame IDs. It is also less than half of the full 8-bits range such that
  // logic can handle wrap around and compare two frame IDs meaningfully.
  kMaxUnackedFrames = 120,

  // The spec declares RTP timestamps must always have a timebase of 90000 ticks
  // per second for video.
  kVideoFrequency = 90000,
};

// Target time interval between the sending of RTCP reports.  Both
// senders and receivers regularly send RTCP reports to their peer.
constexpr base::TimeDelta kRtcpReportInterval = base::Milliseconds(500);

// Success/in-progress/failure status codes reported to clients to indicate
// readiness state.
enum OperationalStatus {
  // Client should not send frames yet (sender), or should not expect to receive
  // frames yet (receiver).
  STATUS_UNINITIALIZED,

  // Client may now send or receive frames.
  STATUS_INITIALIZED,

  // Codec is being re-initialized.  Client may continue sending frames, but
  // some may be ignored/dropped until a transition back to STATUS_INITIALIZED.
  STATUS_CODEC_REINIT_PENDING,

  // Session has halted due to invalid configuration.
  STATUS_INVALID_CONFIGURATION,

  // Session has halted due to an unsupported codec.
  STATUS_UNSUPPORTED_CODEC,

  // Session has halted due to a codec initialization failure.  Note that this
  // can be reported after STATUS_INITIALIZED/STATUS_CODEC_REINIT_PENDING if the
  // codec was re-initialized during the session.
  STATUS_CODEC_INIT_FAILED,

  // Session has halted due to a codec runtime failure.
  STATUS_CODEC_RUNTIME_ERROR,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class CastStreamingFrameDropReason {
  // The frame was not dropped.
  kNotDropped = 0,

  // Reported by the FrameSender implementation.
  kTooManyFramesInFlight = 1,
  kBurstThresholdExceeded = 2,
  kInFlightDurationTooHigh = 3,

  // Reported by openscreen::Sender as the EnqueueFrameResult.
  // Payload is too large, typically meaning several dozen megabytes or more.
  kPayloadTooLarge = 4,
  // Surpassed the max number of FrameIds in flight.
  kReachedIdSpanLimit = 5,
  // Too large of a media duration in flight. Dropping the frame before encoding
  // (kInFlightDurationTooHigh) is strongly preferred, but in some rare cases
  // we may drop the frame after encoding instead.
  kInFlightDurationTooHighAfterEncoding = 6,

  // Reported by the OpenscreenFrameSender.
  kInvalidReferencedFrameId = 7,

  // Should stay updated as the maximum enum value above.
  kMaxValue = kInvalidReferencedFrameId
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_CONSTANTS_H_
