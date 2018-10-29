// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/logging/stats_event_subscriber.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/memory/ref_counted.h"
#include "base/rand_util.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/tick_clock.h"
#include "base/values.h"
#include "media/base/fake_single_thread_task_runner.h"
#include "media/cast/cast_environment.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/test/fake_receiver_time_offset_estimator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const int kReceiverOffsetSecs = 100;
}

namespace media {
namespace cast {

class StatsEventSubscriberTest : public ::testing::Test {
 protected:
  StatsEventSubscriberTest()
      : task_runner_(new FakeSingleThreadTaskRunner(&sender_clock_)),
        cast_environment_(new CastEnvironment(&sender_clock_,
                                              task_runner_,
                                              task_runner_,
                                              task_runner_)),
        fake_offset_estimator_(
            base::TimeDelta::FromSeconds(kReceiverOffsetSecs)) {
    receiver_clock_.Advance(base::TimeDelta::FromSeconds(kReceiverOffsetSecs));
    cast_environment_->logger()->Subscribe(&fake_offset_estimator_);
  }

  ~StatsEventSubscriberTest() override {
    if (subscriber_.get())
      cast_environment_->logger()->Unsubscribe(subscriber_.get());
    cast_environment_->logger()->Unsubscribe(&fake_offset_estimator_);
  }

  void AdvanceClocks(base::TimeDelta delta) {
    task_runner_->Sleep(delta);
    receiver_clock_.Advance(delta);
  }

  void Init(EventMediaType event_media_type) {
    DCHECK(!subscriber_.get());
    subscriber_.reset(new StatsEventSubscriber(
        event_media_type, cast_environment_->Clock(), &fake_offset_estimator_));
    cast_environment_->logger()->Subscribe(subscriber_.get());
  }

  base::SimpleTestTickClock sender_clock_;
  base::SimpleTestTickClock receiver_clock_;
  scoped_refptr<FakeSingleThreadTaskRunner> task_runner_;
  scoped_refptr<CastEnvironment> cast_environment_;
  test::FakeReceiverTimeOffsetEstimator fake_offset_estimator_;
  std::unique_ptr<StatsEventSubscriber> subscriber_;
};

TEST_F(StatsEventSubscriberTest, CaptureEncode) {
  Init(VIDEO_EVENT);

  RtpTimeTicks rtp_timestamp;
  FrameId frame_id = FrameId::first();
  int extra_frames = 50;
  // Only the first |extra_frames| frames logged will be taken into account
  // when computing dropped frames.
  int num_frames = StatsEventSubscriber::kMaxFrameInfoMapSize + 50;
  int dropped_frames = 0;
  base::TimeTicks start_time = sender_clock_.NowTicks();
  // Drop half the frames during the encode step.
  for (int i = 0; i < num_frames; i++) {
    std::unique_ptr<FrameEvent> capture_begin_event(new FrameEvent());
    capture_begin_event->timestamp = sender_clock_.NowTicks();
    capture_begin_event->type = FRAME_CAPTURE_BEGIN;
    capture_begin_event->media_type = VIDEO_EVENT;
    capture_begin_event->rtp_timestamp = rtp_timestamp;
    cast_environment_->logger()->DispatchFrameEvent(
        std::move(capture_begin_event));

    AdvanceClocks(base::TimeDelta::FromMicroseconds(10));
    std::unique_ptr<FrameEvent> capture_end_event(new FrameEvent());
    capture_end_event->timestamp = sender_clock_.NowTicks();
    capture_end_event->type = FRAME_CAPTURE_END;
    capture_end_event->media_type = VIDEO_EVENT;
    capture_end_event->rtp_timestamp = rtp_timestamp;
    cast_environment_->logger()->DispatchFrameEvent(
        std::move(capture_end_event));

    if (i % 2 == 0) {
      AdvanceClocks(base::TimeDelta::FromMicroseconds(10));
      std::unique_ptr<FrameEvent> encode_event(new FrameEvent());
      encode_event->timestamp = sender_clock_.NowTicks();
      encode_event->type = FRAME_ENCODED;
      encode_event->media_type = VIDEO_EVENT;
      encode_event->rtp_timestamp = rtp_timestamp;
      encode_event->frame_id = frame_id;
      encode_event->size = 1024;
      encode_event->key_frame = true;
      encode_event->target_bitrate = 5678;
      encode_event->encoder_cpu_utilization = 9.10;
      encode_event->idealized_bitrate_utilization = 11.12;
      cast_environment_->logger()->DispatchFrameEvent(std::move(encode_event));
    } else if (i < extra_frames) {
      dropped_frames++;
    }
    AdvanceClocks(base::TimeDelta::FromMicroseconds(34567));
    rtp_timestamp += RtpTimeDelta::FromTicks(90);
    frame_id++;
  }

  base::TimeTicks end_time = sender_clock_.NowTicks();

  StatsEventSubscriber::StatsMap stats_map;
  subscriber_->GetStatsInternal(&stats_map);

  auto it = stats_map.find(StatsEventSubscriber::CAPTURE_FPS);
  ASSERT_TRUE(it != stats_map.end());

  base::TimeDelta duration = end_time - start_time;
  EXPECT_DOUBLE_EQ(
      it->second,
      static_cast<double>(num_frames) / duration.InMillisecondsF() * 1000);

  it = stats_map.find(StatsEventSubscriber::NUM_FRAMES_CAPTURED);
  ASSERT_TRUE(it != stats_map.end());

  EXPECT_DOUBLE_EQ(it->second, static_cast<double>(num_frames));

  it = stats_map.find(StatsEventSubscriber::NUM_FRAMES_DROPPED_BY_ENCODER);
  ASSERT_TRUE(it != stats_map.end());

  EXPECT_DOUBLE_EQ(it->second, static_cast<double>(dropped_frames));

  it = stats_map.find(StatsEventSubscriber::AVG_CAPTURE_LATENCY_MS);
  ASSERT_TRUE(it != stats_map.end());

  EXPECT_DOUBLE_EQ(it->second, static_cast<double>(0.01));
}

TEST_F(StatsEventSubscriberTest, Encode) {
  Init(VIDEO_EVENT);

  RtpTimeTicks rtp_timestamp;
  FrameId frame_id = FrameId::first();
  int num_frames = 10;
  base::TimeTicks start_time = sender_clock_.NowTicks();
  AdvanceClocks(base::TimeDelta::FromMicroseconds(35678));
  base::TimeTicks first_event_time = sender_clock_.NowTicks();
  base::TimeTicks last_event_time;
  int total_size = 0;
  for (int i = 0; i < num_frames; i++) {
    int size = 1000 + base::RandInt(-100, 100);
    total_size += size;
    std::unique_ptr<FrameEvent> encode_event(new FrameEvent());
    encode_event->timestamp = sender_clock_.NowTicks();
    encode_event->type = FRAME_ENCODED;
    encode_event->media_type = VIDEO_EVENT;
    encode_event->rtp_timestamp = rtp_timestamp;
    encode_event->frame_id = frame_id;
    encode_event->size = size;
    encode_event->key_frame = true;
    encode_event->target_bitrate = 5678;
    encode_event->encoder_cpu_utilization = 9.10;
    encode_event->idealized_bitrate_utilization = 11.12;
    cast_environment_->logger()->DispatchFrameEvent(std::move(encode_event));
    last_event_time = sender_clock_.NowTicks();

    AdvanceClocks(base::TimeDelta::FromMicroseconds(35678));
    rtp_timestamp += RtpTimeDelta::FromTicks(90);
    frame_id++;
  }

  base::TimeTicks end_time = sender_clock_.NowTicks();

  StatsEventSubscriber::StatsMap stats_map;
  subscriber_->GetStatsInternal(&stats_map);

  auto it = stats_map.find(StatsEventSubscriber::ENCODE_FPS);
  ASSERT_TRUE(it != stats_map.end());

  base::TimeDelta duration = end_time - start_time;
  EXPECT_DOUBLE_EQ(
      it->second,
      static_cast<double>(num_frames) / duration.InMillisecondsF() * 1000);

  it = stats_map.find(StatsEventSubscriber::ENCODE_KBPS);
  ASSERT_TRUE(it != stats_map.end());

  EXPECT_DOUBLE_EQ(it->second,
              static_cast<double>(total_size) / duration.InMillisecondsF() * 8);

  it = stats_map.find(StatsEventSubscriber::FIRST_EVENT_TIME_MS);
  ASSERT_TRUE(it != stats_map.end());

  EXPECT_DOUBLE_EQ(
      it->second,
      (first_event_time - base::TimeTicks::UnixEpoch()).InMillisecondsF());

  it = stats_map.find(StatsEventSubscriber::LAST_EVENT_TIME_MS);
  ASSERT_TRUE(it != stats_map.end());

  EXPECT_DOUBLE_EQ(
      it->second,
      (last_event_time - base::TimeTicks::UnixEpoch()).InMillisecondsF());
}

TEST_F(StatsEventSubscriberTest, Decode) {
  Init(VIDEO_EVENT);

  RtpTimeTicks rtp_timestamp;
  FrameId frame_id = FrameId::first();
  int num_frames = 10;
  base::TimeTicks start_time = sender_clock_.NowTicks();
  for (int i = 0; i < num_frames; i++) {
    std::unique_ptr<FrameEvent> decode_event(new FrameEvent());
    decode_event->timestamp = receiver_clock_.NowTicks();
    decode_event->type = FRAME_DECODED;
    decode_event->media_type = VIDEO_EVENT;
    decode_event->rtp_timestamp = rtp_timestamp;
    decode_event->frame_id = frame_id;
    cast_environment_->logger()->DispatchFrameEvent(std::move(decode_event));

    AdvanceClocks(base::TimeDelta::FromMicroseconds(36789));
    rtp_timestamp += RtpTimeDelta::FromTicks(90);
    frame_id++;
  }

  base::TimeTicks end_time = sender_clock_.NowTicks();

  StatsEventSubscriber::StatsMap stats_map;
  subscriber_->GetStatsInternal(&stats_map);

  auto it = stats_map.find(StatsEventSubscriber::DECODE_FPS);
  ASSERT_TRUE(it != stats_map.end());

  base::TimeDelta duration = end_time - start_time;
  EXPECT_DOUBLE_EQ(
      it->second,
      static_cast<double>(num_frames) / duration.InMillisecondsF() * 1000);
}

TEST_F(StatsEventSubscriberTest, PlayoutDelay) {
  Init(VIDEO_EVENT);

  RtpTimeTicks rtp_timestamp;
  FrameId frame_id = FrameId::first();
  int num_frames = 10;
  int late_frames = 0;
  for (int i = 0, delay_ms = -50; i < num_frames; i++, delay_ms += 10) {
    base::TimeDelta delay = base::TimeDelta::FromMilliseconds(delay_ms);
    if (delay_ms > 0)
      late_frames++;
    std::unique_ptr<FrameEvent> playout_event(new FrameEvent());
    playout_event->timestamp = receiver_clock_.NowTicks();
    playout_event->type = FRAME_PLAYOUT;
    playout_event->media_type = VIDEO_EVENT;
    playout_event->rtp_timestamp = rtp_timestamp;
    playout_event->frame_id = frame_id;
    playout_event->delay_delta = delay;
    cast_environment_->logger()->DispatchFrameEvent(std::move(playout_event));

    AdvanceClocks(base::TimeDelta::FromMicroseconds(37890));
    rtp_timestamp += RtpTimeDelta::FromTicks(90);
    frame_id++;
  }

  StatsEventSubscriber::StatsMap stats_map;
  subscriber_->GetStatsInternal(&stats_map);

  auto it = stats_map.find(StatsEventSubscriber::NUM_FRAMES_LATE);
  ASSERT_TRUE(it != stats_map.end());

  EXPECT_DOUBLE_EQ(it->second, late_frames);
}

TEST_F(StatsEventSubscriberTest, E2ELatency) {
  Init(VIDEO_EVENT);

  RtpTimeTicks rtp_timestamp;
  FrameId frame_id = FrameId::first();
  int num_frames = 10;
  base::TimeDelta total_latency;
  for (int i = 0; i < num_frames; i++) {
    std::unique_ptr<FrameEvent> capture_begin_event(new FrameEvent());
    capture_begin_event->timestamp = sender_clock_.NowTicks();
    capture_begin_event->type = FRAME_CAPTURE_BEGIN;
    capture_begin_event->media_type = VIDEO_EVENT;
    capture_begin_event->rtp_timestamp = rtp_timestamp;
    cast_environment_->logger()->DispatchFrameEvent(
        std::move(capture_begin_event));

    int latency_micros = 100000 + base::RandInt(-5000, 50000);
    base::TimeDelta latency = base::TimeDelta::FromMicroseconds(latency_micros);
    AdvanceClocks(latency);

    int delay_micros = base::RandInt(-50000, 50000);
    base::TimeDelta delay = base::TimeDelta::FromMilliseconds(delay_micros);
    total_latency += latency;

    std::unique_ptr<FrameEvent> playout_event(new FrameEvent());
    playout_event->timestamp = receiver_clock_.NowTicks();
    playout_event->type = FRAME_PLAYOUT;
    playout_event->media_type = VIDEO_EVENT;
    playout_event->rtp_timestamp = rtp_timestamp;
    playout_event->frame_id = frame_id;
    playout_event->delay_delta = delay;
    cast_environment_->logger()->DispatchFrameEvent(std::move(playout_event));

    rtp_timestamp += RtpTimeDelta::FromTicks(90);
    frame_id++;
  }

  StatsEventSubscriber::StatsMap stats_map;
  subscriber_->GetStatsInternal(&stats_map);

  auto it = stats_map.find(StatsEventSubscriber::AVG_E2E_LATENCY_MS);
  ASSERT_TRUE(it != stats_map.end());

  EXPECT_DOUBLE_EQ(
      it->second, total_latency.InMillisecondsF() / num_frames);
}

TEST_F(StatsEventSubscriberTest, Packets) {
  Init(VIDEO_EVENT);

  RtpTimeTicks rtp_timestamp;
  int num_packets = 10;
  int num_latency_recorded_packets = 0;
  base::TimeTicks start_time = sender_clock_.NowTicks();
  int total_size = 0;
  int retransmit_total_size = 0;
  base::TimeDelta total_network_latency;
  base::TimeDelta total_queueing_latency;
  base::TimeDelta total_packet_latency;
  int num_packets_transmitted = 0;
  int num_packets_received = 0;
  int num_packets_retransmitted = 0;
  int num_packets_rtx_rejected = 0;

  base::TimeTicks sender_encoded_time = sender_clock_.NowTicks();
  base::TimeTicks receiver_encoded_time = receiver_clock_.NowTicks();
  std::unique_ptr<FrameEvent> encode_event(new FrameEvent());
  encode_event->timestamp = sender_encoded_time;
  encode_event->type = FRAME_ENCODED;
  encode_event->media_type = VIDEO_EVENT;
  encode_event->rtp_timestamp = rtp_timestamp;
  encode_event->frame_id = FrameId::first();
  cast_environment_->logger()->DispatchFrameEvent(std::move(encode_event));

  // Every 2nd packet will be retransmitted once.
  // Every 4th packet will be retransmitted twice.
  // Every 8th packet will be retransmitted 3 times + 1 rejected retransmission.
  for (int i = 0; i < num_packets; i++) {
    int size = 1000 + base::RandInt(-100, 100);
    total_size += size;

    std::unique_ptr<PacketEvent> send_event(new PacketEvent());
    send_event->timestamp = sender_clock_.NowTicks();
    send_event->type = PACKET_SENT_TO_NETWORK;
    send_event->media_type = VIDEO_EVENT;
    send_event->rtp_timestamp = rtp_timestamp;
    send_event->frame_id = FrameId::first();
    send_event->packet_id = i;
    send_event->max_packet_id = num_packets - 1;
    send_event->size = size;
    cast_environment_->logger()->DispatchPacketEvent(std::move(send_event));

    num_packets_transmitted++;
    total_queueing_latency += sender_clock_.NowTicks() - sender_encoded_time;

    int latency_micros = 20000 + base::RandInt(-10000, 10000);
    base::TimeDelta latency = base::TimeDelta::FromMicroseconds(latency_micros);
    // Latency is only recorded for packets that aren't retransmitted.
    if (i % 2 != 0) {
      total_network_latency += latency;
      total_packet_latency +=
          receiver_clock_.NowTicks() - receiver_encoded_time + latency;
      num_latency_recorded_packets++;
    }

    AdvanceClocks(latency);

    base::TimeTicks received_time = receiver_clock_.NowTicks();

    // Retransmission 1.
    AdvanceClocks(base::TimeDelta::FromMicroseconds(12345));
    if (i % 2 == 0) {
      std::unique_ptr<PacketEvent> retransmit_event(new PacketEvent());
      retransmit_event->timestamp = receiver_clock_.NowTicks();
      retransmit_event->type = PACKET_RETRANSMITTED;
      retransmit_event->media_type = VIDEO_EVENT;
      retransmit_event->rtp_timestamp = rtp_timestamp;
      retransmit_event->frame_id = FrameId::first();
      retransmit_event->packet_id = i;
      retransmit_event->max_packet_id = num_packets - 1;
      retransmit_event->size = size;
      cast_environment_->logger()->DispatchPacketEvent(
          std::move(retransmit_event));

      retransmit_total_size += size;
      num_packets_transmitted++;
      num_packets_retransmitted++;
    }

    // Retransmission 2.
    AdvanceClocks(base::TimeDelta::FromMicroseconds(13456));
    if (i % 4 == 0) {
      std::unique_ptr<PacketEvent> retransmit_event(new PacketEvent());
      retransmit_event->timestamp = receiver_clock_.NowTicks();
      retransmit_event->type = PACKET_RETRANSMITTED;
      retransmit_event->media_type = VIDEO_EVENT;
      retransmit_event->rtp_timestamp = rtp_timestamp;
      retransmit_event->frame_id = FrameId::first();
      retransmit_event->packet_id = i;
      retransmit_event->max_packet_id = num_packets - 1;
      retransmit_event->size = size;
      cast_environment_->logger()->DispatchPacketEvent(
          std::move(retransmit_event));

      retransmit_total_size += size;
      num_packets_transmitted++;
      num_packets_retransmitted++;
    }

    // Retransmission 3.
    AdvanceClocks(base::TimeDelta::FromMicroseconds(14567));
    if (i % 8 == 0) {
      std::unique_ptr<PacketEvent> retransmit_event(new PacketEvent());
      retransmit_event->timestamp = receiver_clock_.NowTicks();
      retransmit_event->type = PACKET_RETRANSMITTED;
      retransmit_event->media_type = VIDEO_EVENT;
      retransmit_event->rtp_timestamp = rtp_timestamp;
      retransmit_event->frame_id = FrameId::first();
      retransmit_event->packet_id = i;
      retransmit_event->max_packet_id = num_packets - 1;
      retransmit_event->size = size;
      cast_environment_->logger()->DispatchPacketEvent(
          std::move(retransmit_event));

      std::unique_ptr<PacketEvent> reject_event(new PacketEvent());
      reject_event->timestamp = receiver_clock_.NowTicks();
      reject_event->type = PACKET_RTX_REJECTED;
      reject_event->media_type = VIDEO_EVENT;
      reject_event->rtp_timestamp = rtp_timestamp;
      reject_event->frame_id = FrameId::first();
      reject_event->packet_id = i;
      reject_event->max_packet_id = num_packets - 1;
      reject_event->size = size;
      cast_environment_->logger()->DispatchPacketEvent(std::move(reject_event));

      retransmit_total_size += size;
      num_packets_transmitted++;
      num_packets_retransmitted++;
      num_packets_rtx_rejected++;
    }

    std::unique_ptr<PacketEvent> receive_event(new PacketEvent());
    receive_event->timestamp = received_time;
    receive_event->type = PACKET_RECEIVED;
    receive_event->media_type = VIDEO_EVENT;
    receive_event->rtp_timestamp = rtp_timestamp;
    receive_event->frame_id = FrameId::first();
    receive_event->packet_id = i;
    receive_event->max_packet_id = num_packets - 1;
    receive_event->size = size;
    cast_environment_->logger()->DispatchPacketEvent(std::move(receive_event));

    num_packets_received++;
  }

  base::TimeTicks end_time = sender_clock_.NowTicks();
  base::TimeDelta duration = end_time - start_time;

  StatsEventSubscriber::StatsMap stats_map;
  subscriber_->GetStatsInternal(&stats_map);

  // Measure AVG_NETWORK_LATENCY_MS, TRANSMISSION_KBPS, RETRANSMISSION_KBPS.
  auto it = stats_map.find(StatsEventSubscriber::AVG_NETWORK_LATENCY_MS);
  ASSERT_TRUE(it != stats_map.end());

  EXPECT_DOUBLE_EQ(
      it->second,
      total_network_latency.InMillisecondsF() / num_latency_recorded_packets);

  it = stats_map.find(StatsEventSubscriber::AVG_QUEUEING_LATENCY_MS);
  ASSERT_TRUE(it != stats_map.end());

  EXPECT_DOUBLE_EQ(
      it->second,
      total_queueing_latency.InMillisecondsF() / num_packets);

  it = stats_map.find(StatsEventSubscriber::AVG_PACKET_LATENCY_MS);
  ASSERT_TRUE(it != stats_map.end());

  EXPECT_DOUBLE_EQ(
      it->second,
      total_packet_latency.InMillisecondsF() / num_latency_recorded_packets);

  it = stats_map.find(StatsEventSubscriber::TRANSMISSION_KBPS);
  ASSERT_TRUE(it != stats_map.end());

  EXPECT_DOUBLE_EQ(it->second,
              static_cast<double>(total_size) / duration.InMillisecondsF() * 8);

  it = stats_map.find(StatsEventSubscriber::RETRANSMISSION_KBPS);
  ASSERT_TRUE(it != stats_map.end());

  EXPECT_DOUBLE_EQ(it->second,
              static_cast<double>(retransmit_total_size) /
                  duration.InMillisecondsF() * 8);

  it = stats_map.find(StatsEventSubscriber::NUM_PACKETS_SENT);
  ASSERT_TRUE(it != stats_map.end());

  EXPECT_DOUBLE_EQ(it->second, static_cast<double>(num_packets));

  it = stats_map.find(StatsEventSubscriber::NUM_PACKETS_RECEIVED);
  ASSERT_TRUE(it != stats_map.end());

  EXPECT_DOUBLE_EQ(it->second, static_cast<double>(num_packets_received));

  it = stats_map.find(StatsEventSubscriber::NUM_PACKETS_RETRANSMITTED);
  ASSERT_TRUE(it != stats_map.end());

  EXPECT_DOUBLE_EQ(it->second, static_cast<double>(num_packets_retransmitted));

  it = stats_map.find(StatsEventSubscriber::NUM_PACKETS_RTX_REJECTED);
  ASSERT_TRUE(it != stats_map.end());

  EXPECT_DOUBLE_EQ(it->second, static_cast<double>(num_packets_rtx_rejected));
}

bool CheckHistogramHasValue(base::ListValue* values,
                            const std::string& bucket, int expected_count) {
  for (size_t i = 0; i < values->GetSize(); ++i) {
    const base::DictionaryValue* dict = NULL;
    values->GetDictionary(i, &dict);
    if (!dict->HasKey(bucket))
      continue;
    int bucket_count = 0;
    if (!dict->GetInteger(bucket, &bucket_count))
      return false;
    return bucket_count == expected_count;
  }
  return false;
}

TEST_F(StatsEventSubscriberTest, Histograms) {
  Init(VIDEO_EVENT);
  AdvanceClocks(base::TimeDelta::FromMilliseconds(123));

  RtpTimeTicks rtp_timestamp = RtpTimeTicks().Expand(UINT32_C(123));
  FrameId frame_id = FrameId::first();

  // 10 Frames with capture latency in the bucket of "10-14"ms.
  // 10 Frames with encode time in the bucket of "15-19"ms.
  for (int i = 0; i < 10; ++i) {
    ++frame_id;
    rtp_timestamp += RtpTimeDelta::FromTicks(1);

    std::unique_ptr<FrameEvent> capture_begin_event(new FrameEvent());
    capture_begin_event->timestamp = sender_clock_.NowTicks();
    capture_begin_event->type = FRAME_CAPTURE_BEGIN;
    capture_begin_event->media_type = VIDEO_EVENT;
    capture_begin_event->rtp_timestamp = rtp_timestamp;
    cast_environment_->logger()->DispatchFrameEvent(
        std::move(capture_begin_event));

    AdvanceClocks(base::TimeDelta::FromMilliseconds(10));
    std::unique_ptr<FrameEvent> capture_end_event(new FrameEvent());
    capture_end_event->timestamp = sender_clock_.NowTicks();
    capture_end_event->type = FRAME_CAPTURE_END;
    capture_end_event->media_type = VIDEO_EVENT;
    capture_end_event->rtp_timestamp = rtp_timestamp;
    cast_environment_->logger()->DispatchFrameEvent(
        std::move(capture_end_event));

    AdvanceClocks(base::TimeDelta::FromMilliseconds(15));
    std::unique_ptr<FrameEvent> encode_event(new FrameEvent());
    encode_event->timestamp = sender_clock_.NowTicks();
    encode_event->type = FRAME_ENCODED;
    encode_event->media_type = VIDEO_EVENT;
    encode_event->rtp_timestamp = rtp_timestamp;
    encode_event->frame_id = frame_id;
    encode_event->size = 1024;
    encode_event->key_frame = true;
    encode_event->target_bitrate = 5678;
    encode_event->encoder_cpu_utilization = 9.10;
    encode_event->idealized_bitrate_utilization = 11.12;
    cast_environment_->logger()->DispatchFrameEvent(std::move(encode_event));
  }

  // Send 3 packets for the last frame.
  // Queueing latencies are 100ms, 200ms and 300ms.
  for (int i = 0; i < 3; ++i) {
    AdvanceClocks(base::TimeDelta::FromMilliseconds(100));
    std::unique_ptr<PacketEvent> send_event(new PacketEvent());
    send_event->timestamp = sender_clock_.NowTicks();
    send_event->type = PACKET_SENT_TO_NETWORK;
    send_event->media_type = VIDEO_EVENT;
    send_event->rtp_timestamp = rtp_timestamp;
    send_event->frame_id = FrameId::first();
    send_event->packet_id = i;
    send_event->max_packet_id = 2;
    send_event->size = 123;
    cast_environment_->logger()->DispatchPacketEvent(std::move(send_event));
  }

  // Receive 3 packets for the last frame.
  // Network latencies are 100ms, 200ms and 300ms.
  // Packet latencies are 400ms.
  AdvanceClocks(base::TimeDelta::FromMilliseconds(100));
  for (int i = 0; i < 3; ++i) {
    std::unique_ptr<PacketEvent> receive_event(new PacketEvent());
    receive_event->timestamp = receiver_clock_.NowTicks();
    receive_event->type = PACKET_RECEIVED;
    receive_event->media_type = VIDEO_EVENT;
    receive_event->rtp_timestamp = rtp_timestamp;
    receive_event->frame_id = FrameId::first();
    receive_event->packet_id = i;
    receive_event->max_packet_id = 2;
    receive_event->size = 123;
    cast_environment_->logger()->DispatchPacketEvent(std::move(receive_event));
  }

  std::unique_ptr<FrameEvent> playout_event(new FrameEvent());
  playout_event->timestamp = receiver_clock_.NowTicks();
  playout_event->type = FRAME_PLAYOUT;
  playout_event->media_type = VIDEO_EVENT;
  playout_event->rtp_timestamp = rtp_timestamp;
  playout_event->frame_id = frame_id;
  playout_event->delay_delta = base::TimeDelta::FromMilliseconds(100);
  cast_environment_->logger()->DispatchFrameEvent(std::move(playout_event));

  StatsEventSubscriber::SimpleHistogram* histogram;
  std::unique_ptr<base::ListValue> values;

  histogram = subscriber_->GetHistogramForTesting(
      StatsEventSubscriber::CAPTURE_LATENCY_MS_HISTO);
  ASSERT_TRUE(histogram);
  values = histogram->GetHistogram();
  EXPECT_TRUE(CheckHistogramHasValue(values.get(), "10-14", 10));

  histogram = subscriber_->GetHistogramForTesting(
      StatsEventSubscriber::ENCODE_TIME_MS_HISTO);
  ASSERT_TRUE(histogram);
  values = histogram->GetHistogram();
  EXPECT_TRUE(CheckHistogramHasValue(values.get(), "15-19", 10));

  histogram = subscriber_->GetHistogramForTesting(
      StatsEventSubscriber::QUEUEING_LATENCY_MS_HISTO);
  ASSERT_TRUE(histogram);
  values = histogram->GetHistogram();
  EXPECT_TRUE(CheckHistogramHasValue(values.get(), "100-119", 1));
  EXPECT_TRUE(CheckHistogramHasValue(values.get(), "200-219", 1));
  EXPECT_TRUE(CheckHistogramHasValue(values.get(), "300-319", 1));

  histogram = subscriber_->GetHistogramForTesting(
      StatsEventSubscriber::NETWORK_LATENCY_MS_HISTO);
  ASSERT_TRUE(histogram);
  values = histogram->GetHistogram();
  EXPECT_TRUE(CheckHistogramHasValue(values.get(), "100-119", 1));
  EXPECT_TRUE(CheckHistogramHasValue(values.get(), "200-219", 1));
  EXPECT_TRUE(CheckHistogramHasValue(values.get(), "300-319", 1));

  histogram = subscriber_->GetHistogramForTesting(
      StatsEventSubscriber::PACKET_LATENCY_MS_HISTO);
  ASSERT_TRUE(histogram);
  values = histogram->GetHistogram();
  EXPECT_TRUE(CheckHistogramHasValue(values.get(), "400-419", 3));

  histogram = subscriber_->GetHistogramForTesting(
      StatsEventSubscriber::LATE_FRAME_MS_HISTO);
  ASSERT_TRUE(histogram);
  values = histogram->GetHistogram();
  EXPECT_TRUE(CheckHistogramHasValue(values.get(), "100-119", 1));
}

}  // namespace cast
}  // namespace media
