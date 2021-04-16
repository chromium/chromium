// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_NET_RTCP_RECEIVER_RTCP_SESSION_H_
#define MEDIA_CAST_NET_RTCP_RECEIVER_RTCP_SESSION_H_

#include "base/time/tick_clock.h"
#include "media/cast/common/clock_drift_smoother.h"
#include "media/cast/net/pacing/paced_sender.h"
#include "media/cast/net/rtcp/receiver_rtcp_event_subscriber.h"
#include "media/cast/net/rtcp/receiver_rtcp_session.h"
#include "media/cast/net/rtcp/rtcp_defines.h"
#include "media/cast/net/rtcp/rtcp_session.h"
#include "media/cast/net/rtcp/rtcp_utility.h"

namespace media {
namespace cast {

// The RTCP session on a receiver handles incoming RTCP SR packets and maintains
// the offset of local clock from the remote clock and lip sync info (RTP
// & NTP timestamps).
class ReceiverRtcpSession : public RtcpSession {
 public:
  ReceiverRtcpSession(const base::TickClock* clock,  // Not owned.
                      uint32_t local_ssrc,
                      uint32_t remote_ssrc);

  ~ReceiverRtcpSession() override;

  uint32_t local_ssrc() const { return local_ssrc_; }
  uint32_t remote_ssrc() const { return remote_ssrc_; }

  // Handle incoming RTCP packet.
  // Returns false if it is not a RTCP packet or it is not directed to
  // this session, e.g. SSRC doesn't match.
  bool IncomingRtcpPacket(const uint8_t* data, size_t length) override;

  // If available, returns true and sets the output arguments to the latest
  // lip-sync timestamps gleaned from the sender reports.  While the sender
  // provides reference NTP times relative to its own wall clock, the
  // |reference_time| returned here has been translated to the local
  // CastEnvironment clock.
  bool GetLatestLipSyncTimes(RtpTimeTicks* rtp_timestamp,
                             base::TimeTicks* reference_time) const;

  uint32_t last_report_truncated_ntp() const {
    return last_report_truncated_ntp_;
  }

  base::TimeTicks time_last_report_received() const {
    return time_last_report_received_;
  }

 private:
  // Received NTP timestamps from RTCP SR packets.
  void OnReceivedNtp(uint32_t ntp_seconds, uint32_t ntp_fraction);

  // Received RTP and NTP timestamps from RTCP SR packets.
  void OnReceivedLipSyncInfo(RtpTimeTicks rtp_timestamp,
                             uint32_t ntp_seconds,
                             uint32_t ntp_fraction);

  const base::TickClock* const clock_;  // Not owned.
  const uint32_t local_ssrc_;
  const uint32_t remote_ssrc_;

  // The truncated (i.e., 64-->32-bit) NTP timestamp provided in the last report
  // from the remote peer, along with the local time at which the report was
  // received.  These values are used for ping-pong'ing NTP timestamps between
  // the peers so that they can estimate the network's round-trip time.
  uint32_t last_report_truncated_ntp_;
  base::TimeTicks time_last_report_received_;

  // Maintains a smoothed offset between the local clock and the remote clock.
  // Calling this member's Current() method is only valid if
  // |time_last_report_received_| has a valid value.
  ClockDriftSmoother local_clock_ahead_by_;

  // Latest "lip sync" info from the sender.  The sender provides the RTP
  // timestamp of some frame of its choosing and also a corresponding reference
  // NTP timestamp sampled from a clock common to all media streams.  It is
  // expected that the sender will update this data regularly and in a timely
  // manner (e.g., about once per second).
  RtpTimeTicks lip_sync_rtp_timestamp_;
  uint64_t lip_sync_ntp_timestamp_;

  // The RTCP packet parser is re-used when parsing each RTCP packet.  It
  // remembers state about prior RTP timestamps and other sequence values to
  // re-construct "expanded" values.
  RtcpParser parser_;

  DISALLOW_COPY_AND_ASSIGN(ReceiverRtcpSession);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_NET_RTCP_RECEIVER_RTCP_SESSION_H_
