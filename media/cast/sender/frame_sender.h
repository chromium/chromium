// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This is the base class for an object that send frames to a receiver.

#ifndef MEDIA_CAST_SENDER_FRAME_SENDER_H_
#define MEDIA_CAST_SENDER_FRAME_SENDER_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/net/cast_transport.h"
#include "media/cast/net/rtcp/rtcp_defines.h"
#include "media/cast/sender/congestion_control.h"

namespace media {
namespace cast {

struct SenderEncodedFrame;

class FrameSender {
 public:
  FrameSender(scoped_refptr<CastEnvironment> cast_environment,
              CastTransport* const transport_sender,
              const FrameSenderConfig& config,
              CongestionControl* congestion_control);
  virtual ~FrameSender();

  int rtp_timebase() const { return rtp_timebase_; }

  // Calling this function is only valid if the receiver supports the
  // "extra_playout_delay", rtp extension.
  void SetTargetPlayoutDelay(base::TimeDelta new_target_playout_delay);

  base::TimeDelta GetTargetPlayoutDelay() const {
    return target_playout_delay_;
  }

  // Called by the encoder with the next EncodeFrame to send.
  void SendEncodedFrame(int requested_bitrate_before_encode,
                        std::unique_ptr<SenderEncodedFrame> encoded_frame);

 protected:
  // Returns the number of frames in the encoder's backlog.
  virtual int GetNumberOfFramesInEncoder() const = 0;

  // Returns the duration of the data in the encoder's backlog plus the duration
  // of sent, unacknowledged frames.
  virtual base::TimeDelta GetInFlightMediaDuration() const = 0;

  // One or more frames were canceled.
  virtual void OnCancelSendingFrames();

 protected:
  class RtcpClient : public RtcpObserver {
   public:
    explicit RtcpClient(base::WeakPtr<FrameSender> frame_sender);
    ~RtcpClient() override;

    void OnReceivedCastMessage(const RtcpCastMessage& cast_message) override;
    void OnReceivedRtt(base::TimeDelta round_trip_time) override;
    void OnReceivedPli() override;

   private:
    const base::WeakPtr<FrameSender> frame_sender_;
  };
  // Schedule and execute periodic sending of RTCP report.
  void ScheduleNextRtcpReport();
  void SendRtcpReport(bool schedule_future_reports);

  // Protected for testability.
  void OnReceivedCastFeedback(const RtcpCastMessage& cast_feedback);

  // Called when a Pli message is received.
  void OnReceivedPli();

  void OnMeasuredRoundTripTime(base::TimeDelta rtt);

  const scoped_refptr<CastEnvironment> cast_environment_;

  // Sends encoded frames over the configured transport (e.g., UDP).  In
  // Chromium, this could be a proxy that first sends the frames from a renderer
  // process to the browser process over IPC, with the browser process being
  // responsible for "packetizing" the frames and pushing packets into the
  // network layer.
  CastTransport* const transport_sender_;

  const uint32_t ssrc_;

 protected:
  // Schedule and execute periodic checks for re-sending packets.  If no
  // acknowledgements have been received for "too long," FrameSender will
  // speculatively re-send certain packets of an unacked frame to kick-start
  // re-transmission.  This is a last resort tactic to prevent the session from
  // getting stuck after a long outage.
  void ScheduleNextResendCheck();
  void ResendCheck();
  void ResendForKickstart();

  // Returns true if too many frames would be in-flight by encoding and sending
  // the next frame having the given |frame_duration|.
  bool ShouldDropNextFrame(base::TimeDelta frame_duration) const;

  // Record or retrieve a recent history of each frame's timestamps.
  // Warning: If a frame ID too far in the past is requested, the getters will
  // silently succeed but return incorrect values.  Be sure to respect
  // media::cast::kMaxUnackedFrames.
  void RecordLatestFrameTimestamps(FrameId frame_id,
                                   base::TimeTicks reference_time,
                                   RtpTimeTicks rtp_timestamp);
  base::TimeTicks GetRecordedReferenceTime(FrameId frame_id) const;
  RtpTimeTicks GetRecordedRtpTimestamp(FrameId frame_id) const;

  // Returns the number of frames that were sent but not yet acknowledged.
  int GetUnacknowledgedFrameCount() const;

  // Playout delay represents total amount of time between a frame's
  // capture/recording on the sender and its playback on the receiver
  // (i.e., shown to a user).  This should be a value large enough to
  // give the system sufficient time to encode, transmit/retransmit,
  // receive, decode, and render; given its run-time environment
  // (sender/receiver hardware performance, network conditions,etc.).

  // The |target_playout delay_| is the current delay that is adaptively
  // adjusted based on feedback from video capture engine and the congestion
  // control. In case of interactive content, the target is adjusted to start
  // at |min_playout_delay_| and in case of animated content, it starts out at
  // |animated_playout_delay_| and then adaptively adjust based on feedback
  // from congestion control.
  base::TimeDelta target_playout_delay_;
  const base::TimeDelta min_playout_delay_;
  const base::TimeDelta max_playout_delay_;
  // Starting playout delay for animated content.
  const base::TimeDelta animated_playout_delay_;

  // If true, we transmit the target playout delay to the receiver.
  bool send_target_playout_delay_;

  // Max encoded frames generated per second.
  double max_frame_rate_;

  // Counts how many RTCP reports are being "aggressively" sent (i.e., one per
  // frame) at the start of the session.  Once a threshold is reached, RTCP
  // reports are instead sent at the configured interval + random drift.
  int num_aggressive_rtcp_reports_sent_;

  // This is "null" until the first frame is sent.  Thereafter, this tracks the
  // last time any frame was sent or re-sent.
  base::TimeTicks last_send_time_;

  // The ID of the last frame sent.  This member is invalid until
  // |!last_send_time_.is_null()|.
  FrameId last_sent_frame_id_;

  // The ID of the latest (not necessarily the last) frame that has been
  // acknowledged.  This member is invalid until |!last_send_time_.is_null()|.
  FrameId latest_acked_frame_id_;

  // Counts the number of duplicate ACK that are being received.  When this
  // number reaches a threshold, the sender will take this as a sign that the
  // receiver hasn't yet received the first packet of the next frame.  In this
  // case, FrameSender will trigger a re-send of the next frame.
  int duplicate_ack_counter_;

  // This object controls how we change the bitrate to make sure the
  // buffer doesn't overflow.
  std::unique_ptr<CongestionControl> congestion_control_;

  // The most recently measured round trip time.
  base::TimeDelta current_round_trip_time_;

  // This flag is set true when a Pli message is received. It is cleared once
  // the FrameSender scheduled an encoded key frame to be sent.
  bool picture_lost_at_receiver_;

 private:
  // Returns the maximum media duration currently allowed in-flight.  This
  // fluctuates in response to the currently-measured network latency.
  base::TimeDelta GetAllowedInFlightMediaDuration() const;

  // RTP timestamp increment representing one second.
  const int rtp_timebase_;

  const bool is_audio_;

  // This is the maximum delay that the sender should get ack from receiver.
  // Otherwise, sender will call ResendForKickstart().
  base::TimeDelta max_ack_delay_;

  // Ring buffers to keep track of recent frame timestamps (both in terms of
  // local reference time and RTP media time).  These should only be accessed
  // through the Record/GetXXX() methods.  The index into this ring
  // buffer is the lower 8 bits of the FrameId.
  base::TimeTicks frame_reference_times_[256];
  RtpTimeTicks frame_rtp_timestamps_[256];

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<FrameSender> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FrameSender);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_SENDER_FRAME_SENDER_H_
