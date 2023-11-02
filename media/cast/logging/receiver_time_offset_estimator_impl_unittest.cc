// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/logging/receiver_time_offset_estimator_impl.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/memory/ref_counted.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "media/base/fake_single_thread_task_runner.h"
#include "media/cast/cast_environment.h"
#include "media/cast/logging/logging_defines.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace cast {

class ReceiverTimeOffsetEstimatorImplTest : public ::testing::Test {
 protected:
  ReceiverTimeOffsetEstimatorImplTest()
      : task_runner_(new FakeSingleThreadTaskRunner(&sender_clock_)),
        cast_environment_(new CastEnvironment(&sender_clock_,
                                              task_runner_,
                                              task_runner_,
                                              task_runner_)) {
    cast_environment_->logger()->Subscribe(&estimator_);
  }

  ~ReceiverTimeOffsetEstimatorImplTest() override {
    cast_environment_->logger()->Unsubscribe(&estimator_);
  }

  void AdvanceClocks(base::TimeDelta time) {
    task_runner_->Sleep(time);
    receiver_clock_.Advance(time);
  }

  base::SimpleTestTickClock sender_clock_;
  scoped_refptr<FakeSingleThreadTaskRunner> task_runner_;
  scoped_refptr<CastEnvironment> cast_environment_;
  base::SimpleTestTickClock receiver_clock_;
  ReceiverTimeOffsetEstimatorImpl estimator_;
};

// Suppose the true offset is 100ms.
// Event A occurred at sender time 20ms.
// Event B occurred at receiver time 130ms. (sender time 30ms)
// Event C occurred at sender time 60ms.
// Then the bound after all 3 events have arrived is [130-60=70, 130-20=110].
TEST_F(ReceiverTimeOffsetEstimatorImplTest, EstimateOffset) {
  int64_t true_offset_ms = 100;
  receiver_clock_.Advance(base::Milliseconds(true_offset_ms));

  base::TimeDelta lower_bound;
  base::TimeDelta upper_bound;

  EXPECT_FALSE(estimator_.GetReceiverOffsetBounds(&lower_bound, &upper_bound));

  const RtpTimeTicks rtp_timestamp;
  FrameId frame_id = FrameId::first();

  AdvanceClocks(base::Milliseconds(20));

  std::unique_ptr<FrameEvent> encode_event(new FrameEvent());
  encode_event->timestamp = sender_clock_.NowTicks();
  encode_event->type = FRAME_ENCODED;
  encode_event->media_type = VIDEO_EVENT;
  encode_event->rtp_timestamp = rtp_timestamp;
  encode_event->frame_id = frame_id;
  encode_event->size = 1234;
  encode_event->key_frame = true;
  encode_event->target_bitrate = 5678;
  encode_event->encoder_cpu_utilization = 9.10;
  encode_event->idealized_bitrate_utilization = 11.12;
  cast_environment_->logger()->DispatchFrameEvent(std::move(encode_event));

  std::unique_ptr<PacketEvent> send_event(new PacketEvent());
  send_event->timestamp = sender_clock_.NowTicks();
  send_event->type = PACKET_SENT_TO_NETWORK;
  send_event->media_type = VIDEO_EVENT;
  send_event->rtp_timestamp = rtp_timestamp;
  send_event->frame_id = frame_id;
  send_event->packet_id = 56;
  send_event->max_packet_id = 78;
  send_event->size = 1500;
  cast_environment_->logger()->DispatchPacketEvent(std::move(send_event));

  EXPECT_FALSE(estimator_.GetReceiverOffsetBounds(&lower_bound, &upper_bound));

  AdvanceClocks(base::Milliseconds(10));
  std::unique_ptr<FrameEvent> ack_sent_event(new FrameEvent());
  ack_sent_event->timestamp = receiver_clock_.NowTicks();
  ack_sent_event->type = FRAME_ACK_SENT;
  ack_sent_event->media_type = VIDEO_EVENT;
  ack_sent_event->rtp_timestamp = rtp_timestamp;
  ack_sent_event->frame_id = frame_id;
  cast_environment_->logger()->DispatchFrameEvent(std::move(ack_sent_event));

  std::unique_ptr<PacketEvent> receive_event(new PacketEvent());
  receive_event->timestamp = receiver_clock_.NowTicks();
  receive_event->type = PACKET_RECEIVED;
  receive_event->media_type = VIDEO_EVENT;
  receive_event->rtp_timestamp = rtp_timestamp;
  receive_event->frame_id = frame_id;
  receive_event->packet_id = 56;
  receive_event->max_packet_id = 78;
  receive_event->size = 1500;
  cast_environment_->logger()->DispatchPacketEvent(std::move(receive_event));

  EXPECT_FALSE(estimator_.GetReceiverOffsetBounds(&lower_bound, &upper_bound));

  AdvanceClocks(base::Milliseconds(30));
  std::unique_ptr<FrameEvent> ack_event(new FrameEvent());
  ack_event->timestamp = sender_clock_.NowTicks();
  ack_event->type = FRAME_ACK_RECEIVED;
  ack_event->media_type = VIDEO_EVENT;
  ack_event->rtp_timestamp = rtp_timestamp;
  ack_event->frame_id = frame_id;
  cast_environment_->logger()->DispatchFrameEvent(std::move(ack_event));

  EXPECT_TRUE(estimator_.GetReceiverOffsetBounds(&lower_bound, &upper_bound));

  int64_t lower_bound_ms = lower_bound.InMilliseconds();
  int64_t upper_bound_ms = upper_bound.InMilliseconds();
  EXPECT_EQ(70, lower_bound_ms);
  EXPECT_EQ(110, upper_bound_ms);
  EXPECT_GE(true_offset_ms, lower_bound_ms);
  EXPECT_LE(true_offset_ms, upper_bound_ms);
}

// Same scenario as above, but event C arrives before event B. It doesn't mean
// event C occurred before event B.
TEST_F(ReceiverTimeOffsetEstimatorImplTest, EventCArrivesBeforeEventB) {
  int64_t true_offset_ms = 100;
  receiver_clock_.Advance(base::Milliseconds(true_offset_ms));

  base::TimeDelta lower_bound;
  base::TimeDelta upper_bound;

  EXPECT_FALSE(estimator_.GetReceiverOffsetBounds(&lower_bound, &upper_bound));

  const RtpTimeTicks rtp_timestamp;
  FrameId frame_id = FrameId::first();

  AdvanceClocks(base::Milliseconds(20));

  std::unique_ptr<FrameEvent> encode_event(new FrameEvent());
  encode_event->timestamp = sender_clock_.NowTicks();
  encode_event->type = FRAME_ENCODED;
  encode_event->media_type = VIDEO_EVENT;
  encode_event->rtp_timestamp = rtp_timestamp;
  encode_event->frame_id = frame_id;
  encode_event->size = 1234;
  encode_event->key_frame = true;
  encode_event->target_bitrate = 5678;
  encode_event->encoder_cpu_utilization = 9.10;
  encode_event->idealized_bitrate_utilization = 11.12;
  cast_environment_->logger()->DispatchFrameEvent(std::move(encode_event));

  std::unique_ptr<PacketEvent> send_event(new PacketEvent());
  send_event->timestamp = sender_clock_.NowTicks();
  send_event->type = PACKET_SENT_TO_NETWORK;
  send_event->media_type = VIDEO_EVENT;
  send_event->rtp_timestamp = rtp_timestamp;
  send_event->frame_id = frame_id;
  send_event->packet_id = 56;
  send_event->max_packet_id = 78;
  send_event->size = 1500;
  cast_environment_->logger()->DispatchPacketEvent(std::move(send_event));

  EXPECT_FALSE(estimator_.GetReceiverOffsetBounds(&lower_bound, &upper_bound));

  AdvanceClocks(base::Milliseconds(10));
  base::TimeTicks event_b_time = receiver_clock_.NowTicks();
  AdvanceClocks(base::Milliseconds(30));
  base::TimeTicks event_c_time = sender_clock_.NowTicks();

  std::unique_ptr<FrameEvent> ack_event(new FrameEvent());
  ack_event->timestamp = event_c_time;
  ack_event->type = FRAME_ACK_RECEIVED;
  ack_event->media_type = VIDEO_EVENT;
  ack_event->rtp_timestamp = rtp_timestamp;
  ack_event->frame_id = frame_id;
  cast_environment_->logger()->DispatchFrameEvent(std::move(ack_event));

  EXPECT_FALSE(estimator_.GetReceiverOffsetBounds(&lower_bound, &upper_bound));

  std::unique_ptr<PacketEvent> receive_event(new PacketEvent());
  receive_event->timestamp = event_b_time;
  receive_event->type = PACKET_RECEIVED;
  receive_event->media_type = VIDEO_EVENT;
  receive_event->rtp_timestamp = rtp_timestamp;
  receive_event->frame_id = frame_id;
  receive_event->packet_id = 56;
  receive_event->max_packet_id = 78;
  receive_event->size = 1500;
  cast_environment_->logger()->DispatchPacketEvent(std::move(receive_event));

  std::unique_ptr<FrameEvent> ack_sent_event(new FrameEvent());
  ack_sent_event->timestamp = event_b_time;
  ack_sent_event->type = FRAME_ACK_SENT;
  ack_sent_event->media_type = VIDEO_EVENT;
  ack_sent_event->rtp_timestamp = rtp_timestamp;
  ack_sent_event->frame_id = frame_id;
  cast_environment_->logger()->DispatchFrameEvent(std::move(ack_sent_event));

  EXPECT_TRUE(estimator_.GetReceiverOffsetBounds(&lower_bound, &upper_bound));

  int64_t lower_bound_ms = lower_bound.InMilliseconds();
  int64_t upper_bound_ms = upper_bound.InMilliseconds();
  EXPECT_EQ(70, lower_bound_ms);
  EXPECT_EQ(110, upper_bound_ms);
  EXPECT_GE(true_offset_ms, lower_bound_ms);
  EXPECT_LE(true_offset_ms, upper_bound_ms);
}

TEST_F(ReceiverTimeOffsetEstimatorImplTest, MultipleIterations) {
  int64_t true_offset_ms = 100;
  receiver_clock_.Advance(base::Milliseconds(true_offset_ms));

  base::TimeDelta lower_bound;
  base::TimeDelta upper_bound;

  const RtpTimeTicks rtp_timestamp_a;
  FrameId frame_id_a = FrameId::first();
  const RtpTimeTicks rtp_timestamp_b =
      rtp_timestamp_a + RtpTimeDelta::FromTicks(90);
  FrameId frame_id_b = frame_id_a + 1;
  const RtpTimeTicks rtp_timestamp_c =
      rtp_timestamp_b + RtpTimeDelta::FromTicks(90);
  FrameId frame_id_c = frame_id_b + 1;

  // Frame 1 times: [20, 30+100, 60]
  // Frame 2 times: [30, 50+100, 55]
  // Frame 3 times: [77, 80+100, 110]
  // Bound should end up at [95, 103]
  // Events times in chronological order: 20, 30 x2, 50, 55, 60, 77, 80, 110
  AdvanceClocks(base::Milliseconds(20));
  std::unique_ptr<FrameEvent> encode_event(new FrameEvent());
  encode_event->timestamp = sender_clock_.NowTicks();
  encode_event->type = FRAME_ENCODED;
  encode_event->media_type = VIDEO_EVENT;
  encode_event->rtp_timestamp = rtp_timestamp_a;
  encode_event->frame_id = frame_id_a;
  encode_event->size = 1234;
  encode_event->key_frame = true;
  encode_event->target_bitrate = 5678;
  encode_event->encoder_cpu_utilization = 9.10;
  encode_event->idealized_bitrate_utilization = 11.12;
  cast_environment_->logger()->DispatchFrameEvent(std::move(encode_event));

  std::unique_ptr<PacketEvent> send_event(new PacketEvent());
  send_event->timestamp = sender_clock_.NowTicks();
  send_event->type = PACKET_SENT_TO_NETWORK;
  send_event->media_type = VIDEO_EVENT;
  send_event->rtp_timestamp = rtp_timestamp_a;
  send_event->frame_id = frame_id_a;
  send_event->packet_id = 56;
  send_event->max_packet_id = 78;
  send_event->size = 1500;
  cast_environment_->logger()->DispatchPacketEvent(std::move(send_event));

  AdvanceClocks(base::Milliseconds(10));
  encode_event = std::make_unique<FrameEvent>();
  encode_event->timestamp = sender_clock_.NowTicks();
  encode_event->type = FRAME_ENCODED;
  encode_event->media_type = VIDEO_EVENT;
  encode_event->rtp_timestamp = rtp_timestamp_b;
  encode_event->frame_id = frame_id_b;
  encode_event->size = 1234;
  encode_event->key_frame = true;
  encode_event->target_bitrate = 5678;
  encode_event->encoder_cpu_utilization = 9.10;
  encode_event->idealized_bitrate_utilization = 11.12;
  cast_environment_->logger()->DispatchFrameEvent(std::move(encode_event));

  send_event = std::make_unique<PacketEvent>();
  send_event->timestamp = sender_clock_.NowTicks();
  send_event->type = PACKET_SENT_TO_NETWORK;
  send_event->media_type = VIDEO_EVENT;
  send_event->rtp_timestamp = rtp_timestamp_b;
  send_event->frame_id = frame_id_b;
  send_event->packet_id = 56;
  send_event->max_packet_id = 78;
  send_event->size = 1500;
  cast_environment_->logger()->DispatchPacketEvent(std::move(send_event));

  std::unique_ptr<FrameEvent> ack_sent_event(new FrameEvent());
  ack_sent_event->timestamp = receiver_clock_.NowTicks();
  ack_sent_event->type = FRAME_ACK_SENT;
  ack_sent_event->media_type = VIDEO_EVENT;
  ack_sent_event->rtp_timestamp = rtp_timestamp_a;
  ack_sent_event->frame_id = frame_id_a;
  cast_environment_->logger()->DispatchFrameEvent(std::move(ack_sent_event));

  AdvanceClocks(base::Milliseconds(20));

  std::unique_ptr<PacketEvent> receive_event(new PacketEvent());
  receive_event->timestamp = receiver_clock_.NowTicks();
  receive_event->type = PACKET_RECEIVED;
  receive_event->media_type = VIDEO_EVENT;
  receive_event->rtp_timestamp = rtp_timestamp_b;
  receive_event->frame_id = frame_id_b;
  receive_event->packet_id = 56;
  receive_event->max_packet_id = 78;
  receive_event->size = 1500;
  cast_environment_->logger()->DispatchPacketEvent(std::move(receive_event));

  ack_sent_event = std::make_unique<FrameEvent>();
  ack_sent_event->timestamp = receiver_clock_.NowTicks();
  ack_sent_event->type = FRAME_ACK_SENT;
  ack_sent_event->media_type = VIDEO_EVENT;
  ack_sent_event->rtp_timestamp = rtp_timestamp_b;
  ack_sent_event->frame_id = frame_id_b;
  cast_environment_->logger()->DispatchFrameEvent(std::move(ack_sent_event));

  AdvanceClocks(base::Milliseconds(5));
  std::unique_ptr<FrameEvent> ack_event(new FrameEvent());
  ack_event->timestamp = sender_clock_.NowTicks();
  ack_event->type = FRAME_ACK_RECEIVED;
  ack_event->media_type = VIDEO_EVENT;
  ack_event->rtp_timestamp = rtp_timestamp_b;
  ack_event->frame_id = frame_id_b;
  cast_environment_->logger()->DispatchFrameEvent(std::move(ack_event));

  AdvanceClocks(base::Milliseconds(5));
  ack_event = std::make_unique<FrameEvent>();
  ack_event->timestamp = sender_clock_.NowTicks();
  ack_event->type = FRAME_ACK_RECEIVED;
  ack_event->media_type = VIDEO_EVENT;
  ack_event->rtp_timestamp = rtp_timestamp_a;
  ack_event->frame_id = frame_id_a;
  cast_environment_->logger()->DispatchFrameEvent(std::move(ack_event));

  AdvanceClocks(base::Milliseconds(17));
  encode_event = std::make_unique<FrameEvent>();
  encode_event->timestamp = sender_clock_.NowTicks();
  encode_event->type = FRAME_ENCODED;
  encode_event->media_type = VIDEO_EVENT;
  encode_event->rtp_timestamp = rtp_timestamp_c;
  encode_event->frame_id = frame_id_c;
  encode_event->size = 1234;
  encode_event->key_frame = true;
  encode_event->target_bitrate = 5678;
  encode_event->encoder_cpu_utilization = 9.10;
  encode_event->idealized_bitrate_utilization = 11.12;
  cast_environment_->logger()->DispatchFrameEvent(std::move(encode_event));

  send_event = std::make_unique<PacketEvent>();
  send_event->timestamp = sender_clock_.NowTicks();
  send_event->type = PACKET_SENT_TO_NETWORK;
  send_event->media_type = VIDEO_EVENT;
  send_event->rtp_timestamp = rtp_timestamp_c;
  send_event->frame_id = frame_id_c;
  send_event->packet_id = 56;
  send_event->max_packet_id = 78;
  send_event->size = 1500;
  cast_environment_->logger()->DispatchPacketEvent(std::move(send_event));

  AdvanceClocks(base::Milliseconds(3));
  receive_event = std::make_unique<PacketEvent>();
  receive_event->timestamp = receiver_clock_.NowTicks();
  receive_event->type = PACKET_RECEIVED;
  receive_event->media_type = VIDEO_EVENT;
  receive_event->rtp_timestamp = rtp_timestamp_c;
  receive_event->frame_id = frame_id_c;
  receive_event->packet_id = 56;
  receive_event->max_packet_id = 78;
  receive_event->size = 1500;
  cast_environment_->logger()->DispatchPacketEvent(std::move(receive_event));

  ack_sent_event = std::make_unique<FrameEvent>();
  ack_sent_event->timestamp = receiver_clock_.NowTicks();
  ack_sent_event->type = FRAME_ACK_SENT;
  ack_sent_event->media_type = VIDEO_EVENT;
  ack_sent_event->rtp_timestamp = rtp_timestamp_c;
  ack_sent_event->frame_id = frame_id_c;
  cast_environment_->logger()->DispatchFrameEvent(std::move(ack_sent_event));

  AdvanceClocks(base::Milliseconds(30));
  ack_event = std::make_unique<FrameEvent>();
  ack_event->timestamp = sender_clock_.NowTicks();
  ack_event->type = FRAME_ACK_RECEIVED;
  ack_event->media_type = VIDEO_EVENT;
  ack_event->rtp_timestamp = rtp_timestamp_c;
  ack_event->frame_id = frame_id_c;
  cast_environment_->logger()->DispatchFrameEvent(std::move(ack_event));

  EXPECT_TRUE(estimator_.GetReceiverOffsetBounds(&lower_bound, &upper_bound));
  int64_t lower_bound_ms = lower_bound.InMilliseconds();
  int64_t upper_bound_ms = upper_bound.InMilliseconds();
  EXPECT_GT(lower_bound_ms, 90);
  EXPECT_LE(lower_bound_ms, true_offset_ms);
  EXPECT_LT(upper_bound_ms, 150);
  EXPECT_GT(upper_bound_ms, true_offset_ms);
}

}  // namespace cast
}  // namespace media
