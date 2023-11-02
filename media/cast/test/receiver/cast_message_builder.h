// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Handles NACK list and manages ACK.

#ifndef MEDIA_CAST_TEST_RECEIVER_CAST_MESSAGE_BUILDER_H_
#define MEDIA_CAST_TEST_RECEIVER_CAST_MESSAGE_BUILDER_H_

#include <stdint.h>

#include <map>

#include "base/containers/circular_deque.h"
#include "base/memory/raw_ptr.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "media/cast/net/rtp/rtp_defines.h"

namespace media {
namespace cast {

class Framer;
class RtpPayloadFeedback;

class CastMessageBuilder {
 public:
  CastMessageBuilder(const base::TickClock* clock,
                     RtpPayloadFeedback* incoming_payload_feedback,
                     const Framer* framer,
                     uint32_t media_ssrc,
                     bool decoder_faster_than_max_frame_rate,
                     int max_unacked_frames);

  CastMessageBuilder(const CastMessageBuilder&) = delete;
  CastMessageBuilder& operator=(const CastMessageBuilder&) = delete;

  ~CastMessageBuilder();

  void CompleteFrameReceived(FrameId frame_id);
  bool TimeToSendNextCastMessage(base::TimeTicks* time_to_send);
  void UpdateCastMessage();

 private:
  bool UpdateAckMessage(FrameId frame_id);
  void BuildPacketList();
  bool UpdateCastMessageInternal(RtcpCastMessage* message);

  FrameId last_acked_frame_id() const { return cast_msg_.ack_frame_id; }

  const raw_ptr<const base::TickClock> clock_;  // Not owned by this class.
  const raw_ptr<RtpPayloadFeedback> cast_feedback_;

  // CastMessageBuilder has only const access to the framer.
  const raw_ptr<const Framer> framer_;
  const uint32_t media_ssrc_;
  const bool decoder_faster_than_max_frame_rate_;
  const int max_unacked_frames_;

  RtcpCastMessage cast_msg_;
  base::TimeTicks last_update_time_;

  std::map<FrameId, base::TimeTicks> time_last_nacked_map_;

  bool slowing_down_ack_;
  bool acked_last_frame_;
  base::circular_deque<FrameId> ack_queue_;
};

}  // namespace cast
}  // namespace media

#endif  //  MEDIA_CAST_TEST_RECEIVER_CAST_MESSAGE_BUILDER_H_
