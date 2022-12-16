// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This is the base class for an object that send frames to a receiver.

#ifndef MEDIA_CAST_SENDER_FRAME_SENDER_IMPL_H_
#define MEDIA_CAST_SENDER_FRAME_SENDER_IMPL_H_

#include <stdint.h>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/net/cast_transport.h"
#include "media/cast/net/rtcp/rtcp_defines.h"
#include "media/cast/sender/congestion_control.h"
#include "media/cast/sender/frame_sender.h"

namespace media::cast {

struct SenderEncodedFrame;

class FrameSenderImpl : public FrameSender {
 public:
  FrameSenderImpl(scoped_refptr<CastEnvironment> cast_environment,
                  const FrameSenderConfig& config,
                  CastTransport* const transport_sender,
                  Client& client);
  ~FrameSenderImpl() override;

  // FrameSender overrides.
  void SetTargetPlayoutDelay(base::TimeDelta new_target_playout_delay) override;
  base::TimeDelta GetTargetPlayoutDelay() const override;
  bool NeedsKeyFrame() const override;
  bool EnqueueFrame(std::unique_ptr<SenderEncodedFrame> encoded_frame) override;
  bool ShouldDropNextFrame(base::TimeDelta frame_duration) const override;
  RtpTimeTicks GetRecordedRtpTimestamp(FrameId frame_id) const override;
  int GetUnacknowledgedFrameCount() const override;
  int GetSuggestedBitrate(base::TimeTicks playout_time,
                          base::TimeDelta playout_delay) override;
  double MaxFrameRate() const override;
  void SetMaxFrameRate(double max_frame_rate) override;
  base::TimeDelta TargetPlayoutDelay() const override;
  base::TimeDelta CurrentRoundTripTime() const override;
  base::TimeTicks LastSendTime() const override;
  FrameId LastAckedFrameId() const override;
  void OnReceivedCastFeedback(const RtcpCastMessage& cast_feedback) override;
  void OnReceivedPli() override;
  void OnMeasuredRoundTripTime(base::TimeDelta rtt) override;

 private:
  // Helper for getting the reference time recorded on the frame associated
  // with |frame_id|.
  base::TimeTicks GetRecordedReferenceTime(FrameId frame_id) const;

  // Schedule and execute periodic checks for re-sending packets.  If no
  // acknowledgements have been received for "too long," FrameSenderImpl will
  // speculatively re-send certain packets of an unacked frame to kick-start
  // re-transmission.  This is a last resort tactic to prevent the session from
  // getting stuck after a long outage.
  void ScheduleNextResendCheck();
  void ResendCheck();
  void ResendForKickstart();

  // Schedule and execute periodic sending of RTCP report.
  void ScheduleNextRtcpReport();
  void SendRtcpReport(bool schedule_future_reports);

  // Record or retrieve a recent history of each frame's timestamps.
  // Warning: If a frame ID too far in the past is requested, the getters will
  // silently succeed but return incorrect values.  Be sure to respect
  // media::cast::kMaxUnackedFrames.
  void RecordLatestFrameTimestamps(FrameId frame_id,
                                   base::TimeTicks reference_time,
                                   RtpTimeTicks rtp_timestamp);

  base::TimeDelta GetInFlightMediaDuration() const;

 private:
  class RtcpClient : public RtcpObserver {
   public:
    explicit RtcpClient(base::WeakPtr<FrameSenderImpl> frame_sender);
    ~RtcpClient() override;

    void OnReceivedCastMessage(const RtcpCastMessage& cast_message) override;
    void OnReceivedRtt(base::TimeDelta round_trip_time) override;
    void OnReceivedPli() override;

   private:
    const base::WeakPtr<FrameSenderImpl> frame_sender_;
  };

  // The cast environment.
  const scoped_refptr<CastEnvironment> cast_environment_;

  // The configuration provided upon initialization.
  const FrameSenderConfig config_;

  // The target playout delay, may fluctuate between the min and max delays
  // stored in |config_|.
  base::TimeDelta target_playout_delay_;

  // Max encoded frames generated per second.
  double max_frame_rate_;

  // Sends encoded frames over the configured transport (e.g., UDP).  In
  // Chromium, this could be a proxy that first sends the frames from a renderer
  // process to the browser process over IPC, with the browser process being
  // responsible for "packetizing" the frames and pushing packets into the
  // network layer.
  const raw_ptr<CastTransport> transport_sender_;

  // The frame sender client.
  const raw_ref<Client> client_;

  // Whether this is an audio or video frame sender.
  const bool is_audio_;

  // The congestion control manages frame statistics and helps make decisions
  // about what bitrate we encode the next frame at.
  std::unique_ptr<CongestionControl> congestion_control_;

  // This is the maximum delay that the sender should get ack from receiver.
  // Otherwise, sender will call ResendForKickstart().
  base::TimeDelta max_ack_delay_;

  // This is "null" until the first frame is sent.  Thereafter, this tracks the
  // last time any frame was sent or re-sent.
  base::TimeTicks last_send_time_;

  // The ID of the last frame sent.  This member is invalid until
  // |!last_send_time_.is_null()|.
  FrameId last_sent_frame_id_;

  // The ID of the latest (not necessarily the last) frame that has been
  // acknowledged.  This member is invalid until |!last_send_time_.is_null()|.
  FrameId latest_acked_frame_id_;

  // The most recently measured round trip time.
  base::TimeDelta current_round_trip_time_;

  // This is the maximum delay that the sender should get ack from receiver.
  // Counts how many RTCP reports are being "aggressively" sent (i.e., one per
  // frame) at the start of the session.  Once a threshold is reached, RTCP
  // reports are instead sent at the configured interval + random drift.
  int num_aggressive_rtcp_reports_sent_ = 0;

  // Counts the number of duplicate ACK that are being received.  When this
  // number reaches a threshold, the sender will take this as a sign that the
  // receiver hasn't yet received the first packet of the next frame.  In this
  // case, FrameSenderImpl will trigger a re-send of the next frame.
  int duplicate_ack_counter_ = 0;

  // This flag is set true when a Pli message is received. It is cleared once
  // the FrameSenderImpl scheduled an encoded key frame to be sent.
  bool picture_lost_at_receiver_ = false;

  // Should send the target playout delay with the next frame.
  bool send_target_playout_delay_ = false;

  // Returns the maximum media duration currently allowed in-flight.  This
  // fluctuates in response to the currently-measured network latency.
  base::TimeDelta GetAllowedInFlightMediaDuration() const;

  // Ring buffers to keep track of recent frame timestamps (both in terms of
  // local reference time and RTP media time).  These should only be accessed
  // through the Record/GetXXX() methods.  The index into this ring
  // buffer is the lower 8 bits of the FrameId.
  base::TimeTicks frame_reference_times_[256];
  RtpTimeTicks frame_rtp_timestamps_[256];

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<FrameSenderImpl> weak_factory_{this};
};

}  // namespace media::cast

#endif  // MEDIA_CAST_SENDER_FRAME_SENDER_IMPL_H_
