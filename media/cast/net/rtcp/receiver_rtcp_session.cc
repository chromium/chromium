// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/net/rtcp/receiver_rtcp_session.h"

#include "base/big_endian.h"
#include "base/logging.h"
#include "base/time/tick_clock.h"
#include "media/cast/net/rtcp/rtcp_builder.h"
#include "media/cast/net/rtcp/rtcp_defines.h"
#include "media/cast/net/rtcp/rtcp_utility.h"

namespace media {
namespace cast {

ReceiverRtcpSession::ReceiverRtcpSession(const base::TickClock* clock,
                                         uint32_t local_ssrc,
                                         uint32_t remote_ssrc)
    : clock_(clock),
      local_ssrc_(local_ssrc),
      remote_ssrc_(remote_ssrc),
      last_report_truncated_ntp_(0),
      local_clock_ahead_by_(ClockDriftSmoother::GetDefaultTimeConstant()),
      lip_sync_ntp_timestamp_(0),
      parser_(local_ssrc, remote_ssrc) {}

ReceiverRtcpSession::~ReceiverRtcpSession() = default;

bool ReceiverRtcpSession::IncomingRtcpPacket(const uint8_t* data,
                                             size_t length) {
  // Check if this is a valid RTCP packet.
  if (!IsRtcpPacket(data, length)) {
    VLOG(1) << "Rtcp@" << this << "::IncomingRtcpPacket() -- "
            << "Received an invalid (non-RTCP?) packet.";
    return false;
  }

  // Check if this packet is to us.
  uint32_t ssrc_of_sender = GetSsrcOfSender(data, length);
  if (ssrc_of_sender != remote_ssrc_) {
    return false;
  }

  // Parse this packet.
  base::BigEndianReader reader(data, length);
  if (parser_.Parse(&reader)) {
    if (parser_.has_sender_report()) {
      OnReceivedNtp(parser_.sender_report().ntp_seconds,
                    parser_.sender_report().ntp_fraction);
      OnReceivedLipSyncInfo(parser_.sender_report().rtp_timestamp,
                            parser_.sender_report().ntp_seconds,
                            parser_.sender_report().ntp_fraction);
    }
  }
  return true;
}

void ReceiverRtcpSession::OnReceivedNtp(uint32_t ntp_seconds,
                                        uint32_t ntp_fraction) {
  last_report_truncated_ntp_ = ConvertToNtpDiff(ntp_seconds, ntp_fraction);

  const base::TimeTicks now = clock_->NowTicks();
  time_last_report_received_ = now;

  const base::TimeDelta measured_offset =
      now - ConvertNtpToTimeTicks(ntp_seconds, ntp_fraction);
  local_clock_ahead_by_.Update(now, measured_offset);
  if (measured_offset < local_clock_ahead_by_.Current()) {
    // Logically, the minimum offset between the clocks has to be the correct
    // one.  For example, the time it took to transmit the current report may
    // have been lower than usual, and so some of the error introduced by the
    // transmission time can be eliminated.
    local_clock_ahead_by_.Reset(now, measured_offset);
  }
  VLOG(1) << "Local clock is ahead of the remote clock by: "
          << "measured=" << measured_offset.InMicroseconds() << " usec, "
          << "filtered=" << local_clock_ahead_by_.Current().InMicroseconds()
          << " usec.";
}

void ReceiverRtcpSession::OnReceivedLipSyncInfo(RtpTimeTicks rtp_timestamp,
                                                uint32_t ntp_seconds,
                                                uint32_t ntp_fraction) {
  CHECK_GT(ntp_seconds, 0u);
  lip_sync_rtp_timestamp_ = rtp_timestamp;
  lip_sync_ntp_timestamp_ =
      (static_cast<uint64_t>(ntp_seconds) << 32) | ntp_fraction;
}

bool ReceiverRtcpSession::GetLatestLipSyncTimes(
    RtpTimeTicks* rtp_timestamp,
    base::TimeTicks* reference_time) const {
  if (!lip_sync_ntp_timestamp_)
    return false;

  const base::TimeTicks local_reference_time =
      ConvertNtpToTimeTicks(
          static_cast<uint32_t>(lip_sync_ntp_timestamp_ >> 32),
          static_cast<uint32_t>(lip_sync_ntp_timestamp_)) +
      local_clock_ahead_by_.Current();

  // Sanity-check: Getting regular lip sync updates?
  DCHECK((clock_->NowTicks() - local_reference_time) < base::Minutes(1));

  *rtp_timestamp = lip_sync_rtp_timestamp_;
  *reference_time = local_reference_time;
  return true;
}

}  // namespace cast
}  // namespace media
