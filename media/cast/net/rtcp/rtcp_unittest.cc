// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/test/simple_test_tick_clock.h"
#include "media/cast/net/cast_transport_config.h"
#include "media/cast/net/pacing/paced_sender.h"
#include "media/cast/net/rtcp/receiver_rtcp_session.h"
#include "media/cast/net/rtcp/rtcp_builder.h"
#include "media/cast/net/rtcp/rtcp_session.h"
#include "media/cast/net/rtcp/rtcp_utility.h"
#include "media/cast/net/rtcp/sender_rtcp_session.h"
#include "media/cast/test/skewed_tick_clock.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {
namespace cast {

namespace {

media::cast::RtcpTimeData CreateRtcpTimeData(base::TimeTicks now) {
  media::cast::RtcpTimeData ret;
  ret.timestamp = now;
  media::cast::ConvertTimeTicksToNtp(now, &ret.ntp_seconds, &ret.ntp_fraction);
  return ret;
}

using testing::_;

static const uint32_t kSenderSsrc = 0x10203;
static const uint32_t kReceiverSsrc = 0x40506;
static const int kInitialReceiverClockOffsetSeconds = -5;
static const uint16_t kTargetDelayMs = 100;

class FakeRtcpTransport : public PacedPacketSender {
 public:
  explicit FakeRtcpTransport(base::SimpleTestTickClock* clock)
      : clock_(clock), packet_delay_(base::TimeDelta::FromMilliseconds(42)) {}

  void set_rtcp_destination(RtcpSession* rtcp_session) {
    rtcp_session_ = rtcp_session;
  }

  base::TimeDelta packet_delay() const { return packet_delay_; }
  void set_packet_delay(base::TimeDelta delay) { packet_delay_ = delay; }

  bool SendRtcpPacket(uint32_t ssrc, PacketRef packet) final {
    clock_->Advance(packet_delay_);
    rtcp_session_->IncomingRtcpPacket(&packet->data[0], packet->data.size());
    return true;
  }

  bool SendPackets(const SendPacketVector& packets) final { return false; }

  bool ResendPackets(const SendPacketVector& packets,
                     const DedupInfo& dedup_info) final {
    return false;
  }

  void CancelSendingPacket(const PacketKey& packet_key) final {}

 private:
  base::SimpleTestTickClock* const clock_;
  base::TimeDelta packet_delay_;
  RtcpSession* rtcp_session_;  //  RTCP destination.

  DISALLOW_COPY_AND_ASSIGN(FakeRtcpTransport);
};

}  // namespace

class RtcpTest : public ::testing::Test, public RtcpObserver {
 protected:
  RtcpTest()
      : sender_clock_(new base::SimpleTestTickClock()),
        receiver_clock_(new test::SkewedTickClock(sender_clock_.get())),
        rtp_sender_pacer_(sender_clock_.get()),
        rtp_receiver_pacer_(sender_clock_.get()),
        rtcp_at_rtp_sender_(sender_clock_.get(),
                            &rtp_sender_pacer_,
                            this,
                            kSenderSsrc,
                            kReceiverSsrc),
        rtcp_at_rtp_receiver_(receiver_clock_.get(),
                              kReceiverSsrc,
                              kSenderSsrc),
        received_pli_(false) {
    sender_clock_->Advance(base::TimeTicks::Now() - base::TimeTicks());
    receiver_clock_->SetSkew(
        1.0,  // No skew.
        base::TimeDelta::FromSeconds(kInitialReceiverClockOffsetSeconds));

    rtp_sender_pacer_.set_rtcp_destination(&rtcp_at_rtp_receiver_);
    rtp_receiver_pacer_.set_rtcp_destination(&rtcp_at_rtp_sender_);
  }

  ~RtcpTest() override = default;

  // RtcpObserver implementation.
  void OnReceivedCastMessage(const RtcpCastMessage& cast_message) override {
    last_cast_message_ = cast_message;
  }
  void OnReceivedRtt(base::TimeDelta round_trip_time) override {
    current_round_trip_time_ = round_trip_time;
  }
  void OnReceivedReceiverLog(const RtcpReceiverLogMessage& logs) override {
    RtcpReceiverLogMessage().swap(last_logs_);

    // Make a copy of the logs.
    for (const RtcpReceiverFrameLogMessage& frame_log_msg : logs) {
      last_logs_.push_back(
          RtcpReceiverFrameLogMessage(frame_log_msg.rtp_timestamp_));
      for (const RtcpReceiverEventLogMessage& event_log_msg :
           frame_log_msg.event_log_messages_) {
        RtcpReceiverEventLogMessage event_log;
        event_log.type = event_log_msg.type;
        event_log.event_timestamp = event_log_msg.event_timestamp;
        event_log.delay_delta = event_log_msg.delay_delta;
        event_log.packet_id = event_log_msg.packet_id;
        last_logs_.back().event_log_messages_.push_back(event_log);
      }
    }
  }

  void OnReceivedPli() override { received_pli_ = true; }

  PacketRef BuildRtcpPacketFromRtpReceiver(
      const RtcpTimeData& time_data,
      const RtcpCastMessage* cast_message,
      const RtcpPliMessage* pli_message,
      base::TimeDelta target_delay,
      const ReceiverRtcpEventSubscriber::RtcpEvents* rtcp_events,
      const RtpReceiverStatistics* rtp_receiver_statistics) {
    RtcpBuilder builder(rtcp_at_rtp_receiver_.local_ssrc());
    builder.Start();
    RtcpReceiverReferenceTimeReport rrtr;
    rrtr.ntp_seconds = time_data.ntp_seconds;
    rrtr.ntp_fraction = time_data.ntp_fraction;
    builder.AddRrtr(rrtr);
    RtcpReportBlock report_block;
    if (rtp_receiver_statistics) {
      report_block.remote_ssrc = 0;
      report_block.media_ssrc = rtcp_at_rtp_receiver_.remote_ssrc();
      report_block.fraction_lost = rtp_receiver_statistics->fraction_lost;
      report_block.cumulative_lost = rtp_receiver_statistics->cumulative_lost;
      report_block.extended_high_sequence_number =
          rtp_receiver_statistics->extended_high_sequence_number;
      report_block.jitter = rtp_receiver_statistics->jitter;
      report_block.last_sr = rtcp_at_rtp_receiver_.last_report_truncated_ntp();
      base::TimeTicks last_report_received_time =
          rtcp_at_rtp_receiver_.time_last_report_received();
      if (!last_report_received_time.is_null()) {
        uint32_t delay_seconds = 0;
        uint32_t delay_fraction = 0;
        base::TimeDelta delta = time_data.timestamp - last_report_received_time;
        ConvertTimeToFractions(delta.InMicroseconds(), &delay_seconds,
                               &delay_fraction);
        report_block.delay_since_last_sr =
            ConvertToNtpDiff(delay_seconds, delay_fraction);
      } else {
        report_block.delay_since_last_sr = 0;
      }
      builder.AddRR(&report_block);
    }
    if (cast_message)
      builder.AddCast(*cast_message, target_delay);
    if (pli_message)
      builder.AddPli(*pli_message);
    if (rtcp_events)
      builder.AddReceiverLog(*rtcp_events);
    return builder.Finish();
  }

  std::unique_ptr<base::SimpleTestTickClock> sender_clock_;
  std::unique_ptr<test::SkewedTickClock> receiver_clock_;
  FakeRtcpTransport rtp_sender_pacer_;
  FakeRtcpTransport rtp_receiver_pacer_;
  SenderRtcpSession rtcp_at_rtp_sender_;
  ReceiverRtcpSession rtcp_at_rtp_receiver_;

  base::TimeDelta current_round_trip_time_;
  RtcpCastMessage last_cast_message_;
  RtcpReceiverLogMessage last_logs_;
  bool received_pli_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RtcpTest);
};

TEST_F(RtcpTest, LipSyncGleanedFromSenderReport) {
  // Initially, expect no lip-sync info receiver-side without having first
  // received a RTCP packet.
  base::TimeTicks reference_time;
  RtpTimeTicks rtp_timestamp;
  ASSERT_FALSE(rtcp_at_rtp_receiver_.GetLatestLipSyncTimes(&rtp_timestamp,
                                                           &reference_time));

  // Send a Sender Report to the receiver.
  const base::TimeTicks reference_time_sent = sender_clock_->NowTicks();
  const RtpTimeTicks rtp_timestamp_sent =
      RtpTimeTicks().Expand(UINT32_C(0xbee5));
  rtcp_at_rtp_sender_.SendRtcpReport(reference_time_sent, rtp_timestamp_sent, 1,
                                     1);

  // Now the receiver should have lip-sync info.  Confirm that the lip-sync
  // reference time is the same as that sent.
  EXPECT_TRUE(rtcp_at_rtp_receiver_.GetLatestLipSyncTimes(&rtp_timestamp,
                                                          &reference_time));
  const base::TimeTicks rolled_back_time =
      (reference_time -
       // Roll-back relative clock offset:
       base::TimeDelta::FromSeconds(kInitialReceiverClockOffsetSeconds) -
       // Roll-back packet transmission time (because RTT is not yet known):
       rtp_sender_pacer_.packet_delay());
  EXPECT_NEAR(0, (reference_time_sent - rolled_back_time).InMicroseconds(), 5);
  EXPECT_EQ(rtp_timestamp_sent, rtp_timestamp);
}

TEST_F(RtcpTest, RoundTripTimesDeterminedFromReportPingPong) {
  const int iterations = 12;

  // Sender does not know the RTT yet.
  ASSERT_EQ(base::TimeDelta(), rtcp_at_rtp_sender_.current_round_trip_time());

  // Do a number of ping-pongs, checking how the round trip times are measured
  // by the sender.
  base::TimeDelta expected_rtt_according_to_sender;
  for (int i = 0; i < iterations; ++i) {
    const base::TimeDelta one_way_trip_time =
        base::TimeDelta::FromMilliseconds(static_cast<int64_t>(1) << i);
    rtp_sender_pacer_.set_packet_delay(one_way_trip_time);
    rtp_receiver_pacer_.set_packet_delay(one_way_trip_time);

    // Sender --> Receiver
    base::TimeTicks reference_time_sent = sender_clock_->NowTicks();
    const RtpTimeTicks rtp_timestamp_sent =
        RtpTimeTicks().Expand<uint32_t>(0xbee5) + RtpTimeDelta::FromTicks(i);
    rtcp_at_rtp_sender_.SendRtcpReport(reference_time_sent, rtp_timestamp_sent,
                                       1, 1);
    EXPECT_EQ(expected_rtt_according_to_sender,
              rtcp_at_rtp_sender_.current_round_trip_time());

    // Validate last reported callback value is same as that reported by method.
    EXPECT_EQ(current_round_trip_time_,
              rtcp_at_rtp_sender_.current_round_trip_time());

    // Receiver --> Sender
    RtpReceiverStatistics stats;
    rtp_receiver_pacer_.SendRtcpPacket(
        rtcp_at_rtp_receiver_.local_ssrc(),
        BuildRtcpPacketFromRtpReceiver(
            CreateRtcpTimeData(receiver_clock_->NowTicks()), nullptr, nullptr,
            base::TimeDelta(), nullptr, &stats));
    expected_rtt_according_to_sender = one_way_trip_time * 2;
    EXPECT_EQ(expected_rtt_according_to_sender,
              rtcp_at_rtp_sender_.current_round_trip_time());
  }
}

TEST_F(RtcpTest, ReportCastFeedback) {
  // Sender has sent all frames up to and including first+5.
  rtcp_at_rtp_sender_.WillSendFrame(FrameId::first() + 5);

  // ACK all frames up to and including first+5, except NACK a few in first+1
  // and first+2.
  RtcpCastMessage cast_message(kSenderSsrc);
  cast_message.ack_frame_id = FrameId::first() + 5;
  PacketIdSet missing_packets1 = {3, 4};
  cast_message.missing_frames_and_packets[FrameId::first() + 1] =
      missing_packets1;
  PacketIdSet missing_packets2 = {5, 6};
  cast_message.missing_frames_and_packets[FrameId::first() + 2] =
      missing_packets2;

  rtp_receiver_pacer_.SendRtcpPacket(
      rtcp_at_rtp_receiver_.local_ssrc(),
      BuildRtcpPacketFromRtpReceiver(
          CreateRtcpTimeData(base::TimeTicks()), &cast_message, nullptr,
          base::TimeDelta::FromMilliseconds(kTargetDelayMs), nullptr, nullptr));

  EXPECT_EQ(last_cast_message_.ack_frame_id, cast_message.ack_frame_id);
  EXPECT_EQ(last_cast_message_.target_delay_ms, kTargetDelayMs);
  EXPECT_EQ(last_cast_message_.missing_frames_and_packets.size(),
            cast_message.missing_frames_and_packets.size());
  EXPECT_TRUE(
      std::equal(cast_message.missing_frames_and_packets.begin(),
                 cast_message.missing_frames_and_packets.end(),
                 last_cast_message_.missing_frames_and_packets.begin()));
}

TEST_F(RtcpTest, ReportPli) {
  RtcpPliMessage pli_message(kSenderSsrc);
  rtp_receiver_pacer_.SendRtcpPacket(
      rtcp_at_rtp_receiver_.local_ssrc(),
      BuildRtcpPacketFromRtpReceiver(CreateRtcpTimeData(base::TimeTicks()),
                                     nullptr, &pli_message, base::TimeDelta(),
                                     nullptr, nullptr));
  EXPECT_TRUE(received_pli_);
}

TEST_F(RtcpTest, DropLateRtcpPacket) {
  // Sender has sent all frames up to and including first+2.
  rtcp_at_rtp_sender_.WillSendFrame(FrameId::first() + 2);

  // Receiver ACKs first+1.
  RtcpCastMessage cast_message(kSenderSsrc);
  cast_message.ack_frame_id = FrameId::first() + 1;
  rtp_receiver_pacer_.SendRtcpPacket(
      rtcp_at_rtp_receiver_.local_ssrc(),
      BuildRtcpPacketFromRtpReceiver(
          CreateRtcpTimeData(receiver_clock_->NowTicks()), &cast_message,
          nullptr, base::TimeDelta::FromMilliseconds(kTargetDelayMs), nullptr,
          nullptr));

  // Receiver ACKs first+2, but with a too-old timestamp.
  RtcpCastMessage late_cast_message(kSenderSsrc);
  late_cast_message.ack_frame_id = FrameId::first() + 2;
  rtp_receiver_pacer_.SendRtcpPacket(
      rtcp_at_rtp_receiver_.local_ssrc(),
      BuildRtcpPacketFromRtpReceiver(
          CreateRtcpTimeData(receiver_clock_->NowTicks() -
                             base::TimeDelta::FromSeconds(10)),
          &late_cast_message, nullptr, base::TimeDelta(), nullptr, nullptr));

  // Validate data from second packet is dropped.
  EXPECT_EQ(last_cast_message_.ack_frame_id, cast_message.ack_frame_id);
  EXPECT_EQ(last_cast_message_.target_delay_ms, kTargetDelayMs);

  // Re-send with fresh timestamp
  late_cast_message.ack_frame_id = FrameId::first() + 2;
  rtp_receiver_pacer_.SendRtcpPacket(
      rtcp_at_rtp_receiver_.local_ssrc(),
      BuildRtcpPacketFromRtpReceiver(
          CreateRtcpTimeData(receiver_clock_->NowTicks()), &late_cast_message,
          nullptr, base::TimeDelta(), nullptr, nullptr));
  EXPECT_EQ(last_cast_message_.ack_frame_id, late_cast_message.ack_frame_id);
  EXPECT_EQ(last_cast_message_.target_delay_ms, 0);
}

TEST_F(RtcpTest, ReportReceiverEvents) {
  const RtpTimeTicks kRtpTimeStamp =
      media::cast::RtpTimeTicks().Expand(UINT32_C(100));
  const base::TimeTicks kEventTimestamp = receiver_clock_->NowTicks();
  const base::TimeDelta kDelayDelta = base::TimeDelta::FromMilliseconds(100);

  RtcpEvent event;
  event.type = FRAME_ACK_SENT;
  event.timestamp = kEventTimestamp;
  event.delay_delta = kDelayDelta;
  ReceiverRtcpEventSubscriber::RtcpEvents rtcp_events;
  rtcp_events.push_back(std::make_pair(kRtpTimeStamp, event));

  rtp_receiver_pacer_.SendRtcpPacket(
      rtcp_at_rtp_receiver_.local_ssrc(),
      BuildRtcpPacketFromRtpReceiver(
          CreateRtcpTimeData(receiver_clock_->NowTicks()), nullptr, nullptr,
          base::TimeDelta(), &rtcp_events, nullptr));

  ASSERT_EQ(1UL, last_logs_.size());
  RtcpReceiverFrameLogMessage frame_log = last_logs_.front();
  EXPECT_EQ(frame_log.rtp_timestamp_, kRtpTimeStamp);

  ASSERT_EQ(1UL, frame_log.event_log_messages_.size());
  RtcpReceiverEventLogMessage log_msg = frame_log.event_log_messages_.back();
  EXPECT_EQ(log_msg.type, event.type);
  EXPECT_EQ(log_msg.delay_delta, event.delay_delta);
  // Only 24 bits of event timestamp sent on wire.
  uint32_t event_ts =
      (event.timestamp - base::TimeTicks()).InMilliseconds() & 0xffffff;
  uint32_t log_msg_ts =
      (log_msg.event_timestamp - base::TimeTicks()).InMilliseconds() & 0xffffff;
  EXPECT_EQ(log_msg_ts, event_ts);
}

}  // namespace cast
}  // namespace media
