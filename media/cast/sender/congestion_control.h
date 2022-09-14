// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_SENDER_CONGESTION_CONTROL_H_
#define MEDIA_CAST_SENDER_CONGESTION_CONTROL_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "media/cast/common/frame_id.h"

namespace media {
namespace cast {

class CongestionControl {
 public:
  virtual ~CongestionControl();

  // Called with latest measured rtt value.
  virtual void UpdateRtt(base::TimeDelta rtt) = 0;

  // Called with an updated target playout delay value.
  virtual void UpdateTargetPlayoutDelay(base::TimeDelta delay) = 0;

  // Called when an encoded frame is enqueued for transport.
  virtual void WillSendFrameToTransport(FrameId frame_id,
                                        size_t frame_size_in_bytes,
                                        base::TimeTicks when) = 0;

  // Called when we receive an ACK for a frame.
  virtual void AckFrame(FrameId frame_id, base::TimeTicks when) = 0;

  // Called when the RTP receiver received frames that have frame ID larger
  // than |last_acked_frame_|.
  virtual void AckLaterFrames(std::vector<FrameId> received_frames,
                              base::TimeTicks when) = 0;

  // Returns the bitrate we should use for the next frame.
  virtual int GetBitrate(base::TimeTicks playout_time,
                         base::TimeDelta playout_delay) = 0;
};

CongestionControl* NewAdaptiveCongestionControl(const base::TickClock* clock,
                                                int max_bitrate_configured,
                                                int min_bitrate_configured,
                                                double max_frame_rate);

CongestionControl* NewFixedCongestionControl(int bitrate);

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_SENDER_CONGESTION_CONTROL_H_
