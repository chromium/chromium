// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/logging/encoding_event_subscriber.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/memory/ref_counted.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/tick_clock.h"
#include "media/base/fake_single_thread_task_runner.h"
#include "media/cast/cast_environment.h"
#include "media/cast/logging/logging_defines.h"
#include "testing/gtest/include/gtest/gtest.h"

using media::cast::proto::AggregatedFrameEvent;
using media::cast::proto::AggregatedPacketEvent;
using media::cast::proto::BasePacketEvent;
using media::cast::proto::LogMetadata;

namespace {

int64_t InMilliseconds(base::TimeTicks event_time) {
  return (event_time - base::TimeTicks()).InMilliseconds();
}

}

namespace media {
namespace cast {

class EncodingEventSubscriberTest : public ::testing::Test {
 protected:
  EncodingEventSubscriberTest()
      : task_runner_(new FakeSingleThreadTaskRunner(&testing_clock_)),
        cast_environment_(new CastEnvironment(&testing_clock_,
                                              task_runner_,
                                              task_runner_,
                                              task_runner_)) {}

  void Init(EventMediaType event_media_type) {
    DCHECK(!event_subscriber_);
    event_subscriber_.reset(new EncodingEventSubscriber(event_media_type, 10));
    cast_environment_->logger()->Subscribe(event_subscriber_.get());
  }

  ~EncodingEventSubscriberTest() override {
    if (event_subscriber_)
      cast_environment_->logger()->Unsubscribe(event_subscriber_.get());
  }

  void GetEventsAndReset() {
    event_subscriber_->GetEventsAndReset(
        &metadata_, &frame_events_, &packet_events_);
    first_rtp_timestamp_ =
        RtpTimeTicks().Expand(metadata_.first_rtp_timestamp());
  }

  base::SimpleTestTickClock testing_clock_;
  scoped_refptr<FakeSingleThreadTaskRunner> task_runner_;
  scoped_refptr<CastEnvironment> cast_environment_;
  std::unique_ptr<EncodingEventSubscriber> event_subscriber_;
  FrameEventList frame_events_;
  PacketEventList packet_events_;
  LogMetadata metadata_;
  RtpTimeTicks first_rtp_timestamp_;
};

TEST_F(EncodingEventSubscriberTest, FrameEventTruncating) {
  Init(VIDEO_EVENT);

  base::TimeTicks now(testing_clock_.NowTicks());

  // Entry with RTP timestamp 0 should get dropped.
  int width = 320;
  int height = 180;
  for (int i = 0; i < 11; i++) {
    std::unique_ptr<FrameEvent> capture_begin_event(new FrameEvent());
    capture_begin_event->timestamp = now;
    capture_begin_event->type = FRAME_CAPTURE_BEGIN;
    capture_begin_event->media_type = VIDEO_EVENT;
    capture_begin_event->rtp_timestamp =
        RtpTimeTicks().Expand<uint32_t>(i * 100);
    cast_environment_->logger()->DispatchFrameEvent(
        std::move(capture_begin_event));

    std::unique_ptr<FrameEvent> capture_end_event(new FrameEvent());
    capture_end_event->timestamp = now;
    capture_end_event->type = FRAME_CAPTURE_END;
    capture_end_event->media_type = VIDEO_EVENT;
    capture_end_event->rtp_timestamp = RtpTimeTicks().Expand<uint32_t>(i * 100);
    capture_end_event->width = width;
    capture_end_event->height = height;
    cast_environment_->logger()->DispatchFrameEvent(
        std::move(capture_end_event));

    std::unique_ptr<FrameEvent> decoded_event(new FrameEvent());
    decoded_event->timestamp = now;
    decoded_event->type = FRAME_DECODED;
    decoded_event->media_type = VIDEO_EVENT;
    decoded_event->rtp_timestamp = RtpTimeTicks().Expand<uint32_t>(i * 100);
    decoded_event->frame_id = FrameId::first();
    cast_environment_->logger()->DispatchFrameEvent(std::move(decoded_event));

    width += 160;
    height += 90;
  }

  GetEventsAndReset();

  ASSERT_EQ(10u, frame_events_.size());
  EXPECT_EQ(100u, frame_events_.front()->relative_rtp_timestamp());
  EXPECT_EQ(1000u, frame_events_.back()->relative_rtp_timestamp());
  width = 320;
  height = 180;
  for (const auto& event : frame_events_) {
    width += 160;
    height += 90;
    EXPECT_EQ(width, event->width());
    EXPECT_EQ(height, event->height());
  }
}

TEST_F(EncodingEventSubscriberTest, PacketEventTruncating) {
  Init(AUDIO_EVENT);

  base::TimeTicks now(testing_clock_.NowTicks());

  // Entry with RTP timestamp 0 should get dropped.
  for (int i = 0; i < 11; i++) {
    std::unique_ptr<PacketEvent> receive_event(new PacketEvent());
    receive_event->timestamp = now;
    receive_event->type = PACKET_RECEIVED;
    receive_event->media_type = AUDIO_EVENT;
    receive_event->rtp_timestamp = RtpTimeTicks().Expand<uint32_t>(i * 100);
    receive_event->frame_id = FrameId::first();
    receive_event->packet_id = i;
    receive_event->max_packet_id = 10;
    receive_event->size = 123;
    cast_environment_->logger()->DispatchPacketEvent(std::move(receive_event));
  }

  GetEventsAndReset();

  ASSERT_EQ(10u, packet_events_.size());
  EXPECT_EQ(100u, packet_events_.front()->relative_rtp_timestamp());
  EXPECT_EQ(1000u, packet_events_.back()->relative_rtp_timestamp());
}

TEST_F(EncodingEventSubscriberTest, TooManyProtos) {
  Init(VIDEO_EVENT);
  size_t num_frame_event_protos = 3;
  size_t num_packet_event_protos = kMaxProtosPerFrame - num_frame_event_protos;
  base::TimeTicks now(testing_clock_.NowTicks());

  for (size_t i = 0; i < num_frame_event_protos; i++) {
    for (int j = 0; j < kMaxEventsPerProto; j++) {
      std::unique_ptr<FrameEvent> capture_begin_event(new FrameEvent());
      capture_begin_event->timestamp = now;
      capture_begin_event->type = FRAME_CAPTURE_BEGIN;
      capture_begin_event->media_type = VIDEO_EVENT;
      capture_begin_event->rtp_timestamp = RtpTimeTicks();
      cast_environment_->logger()->DispatchFrameEvent(
          std::move(capture_begin_event));
    }
  }

  for (size_t i = 0; i < num_packet_event_protos; i++) {
    for (int j = 0; j < kMaxEventsPerProto; j++) {
      std::unique_ptr<PacketEvent> receive_event(new PacketEvent());
      receive_event->timestamp = now;
      receive_event->type = PACKET_RECEIVED;
      receive_event->media_type = VIDEO_EVENT;
      receive_event->rtp_timestamp = RtpTimeTicks();
      receive_event->frame_id = FrameId::first();
      receive_event->packet_id = 0;
      receive_event->max_packet_id = 10;
      receive_event->size = 123;
      cast_environment_->logger()->DispatchPacketEvent(
          std::move(receive_event));
    }
  }

  std::unique_ptr<FrameEvent> capture_begin_event(new FrameEvent());
  capture_begin_event->timestamp = now;
  capture_begin_event->type = FRAME_CAPTURE_BEGIN;
  capture_begin_event->media_type = VIDEO_EVENT;
  capture_begin_event->rtp_timestamp = RtpTimeTicks();
  cast_environment_->logger()->DispatchFrameEvent(
      std::move(capture_begin_event));

  GetEventsAndReset();
  EXPECT_EQ(num_frame_event_protos, frame_events_.size());
  EXPECT_EQ(num_packet_event_protos, packet_events_.size());
}

TEST_F(EncodingEventSubscriberTest, EventFiltering) {
  Init(VIDEO_EVENT);

  base::TimeTicks now(testing_clock_.NowTicks());
  RtpTimeTicks rtp_timestamp = RtpTimeTicks().Expand(UINT32_C(100));
  std::unique_ptr<FrameEvent> video_event(new FrameEvent());
  video_event->timestamp = now;
  video_event->type = FRAME_DECODED;
  video_event->media_type = VIDEO_EVENT;
  video_event->rtp_timestamp = rtp_timestamp;
  video_event->frame_id = FrameId::first();
  cast_environment_->logger()->DispatchFrameEvent(std::move(video_event));

  // This is an AUDIO_EVENT and shouldn't be processed by the subscriber.
  std::unique_ptr<FrameEvent> audio_event(new FrameEvent());
  audio_event->timestamp = now;
  audio_event->type = FRAME_DECODED;
  audio_event->media_type = AUDIO_EVENT;
  audio_event->rtp_timestamp = rtp_timestamp;
  audio_event->frame_id = FrameId::first();
  cast_environment_->logger()->DispatchFrameEvent(std::move(audio_event));

  GetEventsAndReset();

  ASSERT_EQ(1u, frame_events_.size());
  auto it = frame_events_.begin();

  const AggregatedFrameEvent* frame_event = it->get();

  ASSERT_EQ(1, frame_event->event_type_size());
  EXPECT_EQ(media::cast::proto::FRAME_DECODED,
            frame_event->event_type(0));

  GetEventsAndReset();

  EXPECT_TRUE(packet_events_.empty());
}

TEST_F(EncodingEventSubscriberTest, FrameEvent) {
  Init(VIDEO_EVENT);
  base::TimeTicks now(testing_clock_.NowTicks());
  RtpTimeTicks rtp_timestamp = RtpTimeTicks().Expand(UINT32_C(100));
  std::unique_ptr<FrameEvent> decode_event(new FrameEvent());
  decode_event->timestamp = now;
  decode_event->type = FRAME_DECODED;
  decode_event->media_type = VIDEO_EVENT;
  decode_event->rtp_timestamp = rtp_timestamp;
  decode_event->frame_id = FrameId::first();
  cast_environment_->logger()->DispatchFrameEvent(std::move(decode_event));

  GetEventsAndReset();

  ASSERT_EQ(1u, frame_events_.size());

  auto it = frame_events_.begin();

  const AggregatedFrameEvent* event = it->get();

  EXPECT_EQ((rtp_timestamp - first_rtp_timestamp_).lower_32_bits(),
            event->relative_rtp_timestamp());

  ASSERT_EQ(1, event->event_type_size());
  EXPECT_EQ(media::cast::proto::FRAME_DECODED, event->event_type(0));
  ASSERT_EQ(1, event->event_timestamp_ms_size());
  EXPECT_EQ(InMilliseconds(now), event->event_timestamp_ms(0));

  EXPECT_EQ(0, event->encoded_frame_size());
  EXPECT_EQ(0, event->delay_millis());

  GetEventsAndReset();
  EXPECT_TRUE(frame_events_.empty());
}

TEST_F(EncodingEventSubscriberTest, FrameEventDelay) {
  Init(AUDIO_EVENT);
  base::TimeTicks now(testing_clock_.NowTicks());
  RtpTimeTicks rtp_timestamp = RtpTimeTicks().Expand(UINT32_C(100));
  int delay_ms = 100;
  std::unique_ptr<FrameEvent> playout_event(new FrameEvent());
  playout_event->timestamp = now;
  playout_event->type = FRAME_PLAYOUT;
  playout_event->media_type = AUDIO_EVENT;
  playout_event->rtp_timestamp = rtp_timestamp;
  playout_event->frame_id = FrameId::first();
  playout_event->delay_delta = base::TimeDelta::FromMilliseconds(delay_ms);
  cast_environment_->logger()->DispatchFrameEvent(std::move(playout_event));

  GetEventsAndReset();

  ASSERT_EQ(1u, frame_events_.size());

  auto it = frame_events_.begin();

  const AggregatedFrameEvent* event = it->get();

  EXPECT_EQ((rtp_timestamp - first_rtp_timestamp_).lower_32_bits(),
            event->relative_rtp_timestamp());

  ASSERT_EQ(1, event->event_type_size());
  EXPECT_EQ(media::cast::proto::FRAME_PLAYOUT, event->event_type(0));
  ASSERT_EQ(1, event->event_timestamp_ms_size());
  EXPECT_EQ(InMilliseconds(now), event->event_timestamp_ms(0));

  EXPECT_EQ(0, event->encoded_frame_size());
  EXPECT_EQ(100, event->delay_millis());
  EXPECT_FALSE(event->has_key_frame());
}

TEST_F(EncodingEventSubscriberTest, FrameEventSize) {
  Init(VIDEO_EVENT);
  base::TimeTicks now(testing_clock_.NowTicks());
  RtpTimeTicks rtp_timestamp = RtpTimeTicks().Expand(UINT32_C(100));
  int size = 123;
  bool key_frame = true;
  int target_bitrate = 1024;
  double encoder_cpu_utilization = 0.90;
  double idealized_bitrate_utilization = 0.42;
  std::unique_ptr<FrameEvent> encode_event(new FrameEvent());
  encode_event->timestamp = now;
  encode_event->type = FRAME_ENCODED;
  encode_event->media_type = VIDEO_EVENT;
  encode_event->rtp_timestamp = rtp_timestamp;
  encode_event->frame_id = FrameId::first();
  encode_event->size = size;
  encode_event->key_frame = key_frame;
  encode_event->target_bitrate = target_bitrate;
  encode_event->encoder_cpu_utilization = encoder_cpu_utilization;
  encode_event->idealized_bitrate_utilization = idealized_bitrate_utilization;
  cast_environment_->logger()->DispatchFrameEvent(std::move(encode_event));

  GetEventsAndReset();

  ASSERT_EQ(1u, frame_events_.size());

  auto it = frame_events_.begin();

  const AggregatedFrameEvent* event = it->get();

  EXPECT_EQ((rtp_timestamp - first_rtp_timestamp_).lower_32_bits(),
            event->relative_rtp_timestamp());

  ASSERT_EQ(1, event->event_type_size());
  EXPECT_EQ(media::cast::proto::FRAME_ENCODED, event->event_type(0));
  ASSERT_EQ(1, event->event_timestamp_ms_size());
  EXPECT_EQ(InMilliseconds(now), event->event_timestamp_ms(0));

  EXPECT_EQ(size, event->encoded_frame_size());
  EXPECT_EQ(0, event->delay_millis());
  EXPECT_TRUE(event->has_key_frame());
  EXPECT_EQ(key_frame, event->key_frame());
  EXPECT_EQ(target_bitrate, event->target_bitrate());
  EXPECT_EQ(90, event->encoder_cpu_percent_utilized());
  EXPECT_EQ(42, event->idealized_bitrate_percent_utilized());
}

TEST_F(EncodingEventSubscriberTest, MultipleFrameEvents) {
  Init(AUDIO_EVENT);
  RtpTimeTicks rtp_timestamp1 = RtpTimeTicks().Expand(UINT32_C(100));
  RtpTimeTicks rtp_timestamp2 = rtp_timestamp1.Expand(UINT32_C(200));
  base::TimeTicks now1(testing_clock_.NowTicks());
  std::unique_ptr<FrameEvent> playout_event(new FrameEvent());
  playout_event->timestamp = now1;
  playout_event->type = FRAME_PLAYOUT;
  playout_event->media_type = AUDIO_EVENT;
  playout_event->rtp_timestamp = rtp_timestamp1;
  playout_event->frame_id = FrameId::first();
  playout_event->delay_delta = base::TimeDelta::FromMilliseconds(100);
  cast_environment_->logger()->DispatchFrameEvent(std::move(playout_event));

  task_runner_->Sleep(base::TimeDelta::FromMilliseconds(20));
  base::TimeTicks now2(testing_clock_.NowTicks());
  std::unique_ptr<FrameEvent> encode_event(new FrameEvent());
  encode_event->timestamp = now2;
  encode_event->type = FRAME_ENCODED;
  encode_event->media_type = AUDIO_EVENT;
  encode_event->rtp_timestamp = rtp_timestamp2;
  encode_event->frame_id = FrameId::first();
  encode_event->size = 123;
  encode_event->encoder_cpu_utilization = 0.44;
  encode_event->idealized_bitrate_utilization = 0.55;
  cast_environment_->logger()->DispatchFrameEvent(std::move(encode_event));

  task_runner_->Sleep(base::TimeDelta::FromMilliseconds(20));
  base::TimeTicks now3(testing_clock_.NowTicks());
  std::unique_ptr<FrameEvent> decode_event(new FrameEvent());
  decode_event->timestamp = now3;
  decode_event->type = FRAME_DECODED;
  decode_event->media_type = AUDIO_EVENT;
  decode_event->rtp_timestamp = rtp_timestamp1;
  decode_event->frame_id = FrameId::first();
  cast_environment_->logger()->DispatchFrameEvent(std::move(decode_event));

  GetEventsAndReset();

  ASSERT_EQ(2u, frame_events_.size());

  auto it = frame_events_.begin();

  {
    const AggregatedFrameEvent* event = it->get();

    EXPECT_EQ((rtp_timestamp1 - first_rtp_timestamp_).lower_32_bits(),
              event->relative_rtp_timestamp());

    ASSERT_EQ(2, event->event_type_size());
    EXPECT_EQ(media::cast::proto::FRAME_PLAYOUT, event->event_type(0));
    EXPECT_EQ(media::cast::proto::FRAME_DECODED, event->event_type(1));

    ASSERT_EQ(2, event->event_timestamp_ms_size());
    EXPECT_EQ(InMilliseconds(now1), event->event_timestamp_ms(0));
    EXPECT_EQ(InMilliseconds(now3), event->event_timestamp_ms(1));

    EXPECT_FALSE(event->has_key_frame());
  }

  ++it;

  {
    const AggregatedFrameEvent* event = it->get();

    EXPECT_EQ((rtp_timestamp2 - first_rtp_timestamp_).lower_32_bits(),
              event->relative_rtp_timestamp());

    ASSERT_EQ(1, event->event_type_size());
    EXPECT_EQ(media::cast::proto::FRAME_ENCODED, event->event_type(0));

    ASSERT_EQ(1, event->event_timestamp_ms_size());
    EXPECT_EQ(InMilliseconds(now2), event->event_timestamp_ms(0));

    EXPECT_FALSE(event->has_key_frame());
    EXPECT_EQ(44, event->encoder_cpu_percent_utilized());
    EXPECT_EQ(55, event->idealized_bitrate_percent_utilized());
  }
}

TEST_F(EncodingEventSubscriberTest, PacketEvent) {
  Init(AUDIO_EVENT);
  base::TimeTicks now(testing_clock_.NowTicks());
  RtpTimeTicks rtp_timestamp = RtpTimeTicks().Expand(UINT32_C(100));
  int packet_id = 2;
  int size = 100;
  std::unique_ptr<PacketEvent> receive_event(new PacketEvent());
  receive_event->timestamp = now;
  receive_event->type = PACKET_RECEIVED;
  receive_event->media_type = AUDIO_EVENT;
  receive_event->rtp_timestamp = rtp_timestamp;
  receive_event->frame_id = FrameId::first();
  receive_event->packet_id = packet_id;
  receive_event->max_packet_id = 10;
  receive_event->size = size;
  cast_environment_->logger()->DispatchPacketEvent(std::move(receive_event));

  GetEventsAndReset();

  ASSERT_EQ(1u, packet_events_.size());

  auto it = packet_events_.begin();

  const AggregatedPacketEvent* event = it->get();

  EXPECT_EQ((rtp_timestamp - first_rtp_timestamp_).lower_32_bits(),
            event->relative_rtp_timestamp());

  ASSERT_EQ(1, event->base_packet_event_size());
  const BasePacketEvent& base_event = event->base_packet_event(0);
  EXPECT_EQ(packet_id, base_event.packet_id());
  ASSERT_EQ(1, base_event.event_type_size());
  EXPECT_EQ(media::cast::proto::PACKET_RECEIVED,
            base_event.event_type(0));
  ASSERT_EQ(1, base_event.event_timestamp_ms_size());
  EXPECT_EQ(InMilliseconds(now), base_event.event_timestamp_ms(0));
  EXPECT_EQ(size, base_event.size());

  GetEventsAndReset();
  EXPECT_TRUE(packet_events_.empty());
}

TEST_F(EncodingEventSubscriberTest, MultiplePacketEventsForPacket) {
  Init(VIDEO_EVENT);
  base::TimeTicks now1(testing_clock_.NowTicks());
  RtpTimeTicks rtp_timestamp = RtpTimeTicks().Expand(UINT32_C(100));
  int packet_id = 2;
  int size = 100;
  std::unique_ptr<PacketEvent> send_event(new PacketEvent());
  send_event->timestamp = now1;
  send_event->type = PACKET_SENT_TO_NETWORK;
  send_event->media_type = VIDEO_EVENT;
  send_event->rtp_timestamp = rtp_timestamp;
  send_event->frame_id = FrameId::first();
  send_event->packet_id = packet_id;
  send_event->max_packet_id = 10;
  send_event->size = size;
  cast_environment_->logger()->DispatchPacketEvent(std::move(send_event));

  task_runner_->Sleep(base::TimeDelta::FromMilliseconds(20));
  base::TimeTicks now2(testing_clock_.NowTicks());
  std::unique_ptr<PacketEvent> retransmit_event(new PacketEvent());
  retransmit_event->timestamp = now2;
  retransmit_event->type = PACKET_RETRANSMITTED;
  retransmit_event->media_type = VIDEO_EVENT;
  retransmit_event->rtp_timestamp = rtp_timestamp;
  retransmit_event->frame_id = FrameId::first();
  retransmit_event->packet_id = packet_id;
  retransmit_event->max_packet_id = 10;
  retransmit_event->size = size;
  cast_environment_->logger()->DispatchPacketEvent(std::move(retransmit_event));

  GetEventsAndReset();

  ASSERT_EQ(1u, packet_events_.size());

  auto it = packet_events_.begin();

  const AggregatedPacketEvent* event = it->get();

  EXPECT_EQ((rtp_timestamp - first_rtp_timestamp_).lower_32_bits(),
            event->relative_rtp_timestamp());

  ASSERT_EQ(1, event->base_packet_event_size());
  const BasePacketEvent& base_event = event->base_packet_event(0);
  EXPECT_EQ(packet_id, base_event.packet_id());
  ASSERT_EQ(2, base_event.event_type_size());
  EXPECT_EQ(media::cast::proto::PACKET_SENT_TO_NETWORK,
            base_event.event_type(0));
  EXPECT_EQ(media::cast::proto::PACKET_RETRANSMITTED,
            base_event.event_type(1));
  ASSERT_EQ(2, base_event.event_timestamp_ms_size());
  EXPECT_EQ(InMilliseconds(now1), base_event.event_timestamp_ms(0));
  EXPECT_EQ(InMilliseconds(now2), base_event.event_timestamp_ms(1));
}

TEST_F(EncodingEventSubscriberTest, MultiplePacketEventsForFrame) {
  Init(VIDEO_EVENT);
  base::TimeTicks now1(testing_clock_.NowTicks());
  RtpTimeTicks rtp_timestamp = RtpTimeTicks().Expand(UINT32_C(100));
  int packet_id_1 = 2;
  int packet_id_2 = 3;
  int size = 100;
  std::unique_ptr<PacketEvent> send_event(new PacketEvent());
  send_event->timestamp = now1;
  send_event->type = PACKET_SENT_TO_NETWORK;
  send_event->media_type = VIDEO_EVENT;
  send_event->rtp_timestamp = rtp_timestamp;
  send_event->frame_id = FrameId::first();
  send_event->packet_id = packet_id_1;
  send_event->max_packet_id = 10;
  send_event->size = size;
  cast_environment_->logger()->DispatchPacketEvent(std::move(send_event));

  task_runner_->Sleep(base::TimeDelta::FromMilliseconds(20));
  base::TimeTicks now2(testing_clock_.NowTicks());
  std::unique_ptr<PacketEvent> retransmit_event(new PacketEvent());
  retransmit_event->timestamp = now2;
  retransmit_event->type = PACKET_RETRANSMITTED;
  retransmit_event->media_type = VIDEO_EVENT;
  retransmit_event->rtp_timestamp = rtp_timestamp;
  retransmit_event->frame_id = FrameId::first();
  retransmit_event->packet_id = packet_id_2;
  retransmit_event->max_packet_id = 10;
  retransmit_event->size = size;
  cast_environment_->logger()->DispatchPacketEvent(std::move(retransmit_event));

  GetEventsAndReset();

  ASSERT_EQ(1u, packet_events_.size());

  auto it = packet_events_.begin();

  const AggregatedPacketEvent* event = it->get();

  EXPECT_EQ((rtp_timestamp - first_rtp_timestamp_).lower_32_bits(),
            event->relative_rtp_timestamp());

  ASSERT_EQ(2, event->base_packet_event_size());
  const BasePacketEvent& base_event = event->base_packet_event(0);
  EXPECT_EQ(packet_id_1, base_event.packet_id());
  ASSERT_EQ(1, base_event.event_type_size());
  EXPECT_EQ(media::cast::proto::PACKET_SENT_TO_NETWORK,
            base_event.event_type(0));
  ASSERT_EQ(1, base_event.event_timestamp_ms_size());
  EXPECT_EQ(InMilliseconds(now1), base_event.event_timestamp_ms(0));

  const BasePacketEvent& base_event_2 = event->base_packet_event(1);
  EXPECT_EQ(packet_id_2, base_event_2.packet_id());
  ASSERT_EQ(1, base_event_2.event_type_size());
  EXPECT_EQ(media::cast::proto::PACKET_RETRANSMITTED,
            base_event_2.event_type(0));
  ASSERT_EQ(1, base_event_2.event_timestamp_ms_size());
  EXPECT_EQ(InMilliseconds(now2), base_event_2.event_timestamp_ms(0));
}

TEST_F(EncodingEventSubscriberTest, MultiplePacketEvents) {
  Init(VIDEO_EVENT);
  base::TimeTicks now1(testing_clock_.NowTicks());
  RtpTimeTicks rtp_timestamp_1 = RtpTimeTicks().Expand(UINT32_C(100));
  RtpTimeTicks rtp_timestamp_2 = rtp_timestamp_1.Expand(UINT32_C(200));
  int packet_id_1 = 2;
  int packet_id_2 = 3;
  int size = 100;
  std::unique_ptr<PacketEvent> send_event(new PacketEvent());
  send_event->timestamp = now1;
  send_event->type = PACKET_SENT_TO_NETWORK;
  send_event->media_type = VIDEO_EVENT;
  send_event->rtp_timestamp = rtp_timestamp_1;
  send_event->frame_id = FrameId::first();
  send_event->packet_id = packet_id_1;
  send_event->max_packet_id = 10;
  send_event->size = size;
  cast_environment_->logger()->DispatchPacketEvent(std::move(send_event));

  task_runner_->Sleep(base::TimeDelta::FromMilliseconds(20));
  base::TimeTicks now2(testing_clock_.NowTicks());
  std::unique_ptr<PacketEvent> retransmit_event(new PacketEvent());
  retransmit_event->timestamp = now2;
  retransmit_event->type = PACKET_RETRANSMITTED;
  retransmit_event->media_type = VIDEO_EVENT;
  retransmit_event->rtp_timestamp = rtp_timestamp_2;
  retransmit_event->frame_id = FrameId::first();
  retransmit_event->packet_id = packet_id_2;
  retransmit_event->max_packet_id = 10;
  retransmit_event->size = size;
  cast_environment_->logger()->DispatchPacketEvent(std::move(retransmit_event));

  GetEventsAndReset();

  ASSERT_EQ(2u, packet_events_.size());

  auto it = packet_events_.begin();

  {
    const AggregatedPacketEvent* event = it->get();

    EXPECT_EQ((rtp_timestamp_1 - first_rtp_timestamp_).lower_32_bits(),
              event->relative_rtp_timestamp());

    ASSERT_EQ(1, event->base_packet_event_size());
    const BasePacketEvent& base_event = event->base_packet_event(0);
    EXPECT_EQ(packet_id_1, base_event.packet_id());
    ASSERT_EQ(1, base_event.event_type_size());
    EXPECT_EQ(media::cast::proto::PACKET_SENT_TO_NETWORK,
              base_event.event_type(0));
    ASSERT_EQ(1, base_event.event_timestamp_ms_size());
    EXPECT_EQ(InMilliseconds(now1), base_event.event_timestamp_ms(0));
  }

  ++it;
  ASSERT_TRUE(it != packet_events_.end());

  {
    const AggregatedPacketEvent* event = it->get();

    EXPECT_EQ((rtp_timestamp_2 - first_rtp_timestamp_).lower_32_bits(),
              event->relative_rtp_timestamp());

    ASSERT_EQ(1, event->base_packet_event_size());
    const BasePacketEvent& base_event_2 = event->base_packet_event(0);
    EXPECT_EQ(packet_id_2, base_event_2.packet_id());
    ASSERT_EQ(1, base_event_2.event_type_size());
    EXPECT_EQ(media::cast::proto::PACKET_RETRANSMITTED,
              base_event_2.event_type(0));
    ASSERT_EQ(1, base_event_2.event_timestamp_ms_size());
    EXPECT_EQ(InMilliseconds(now2), base_event_2.event_timestamp_ms(0));
  }
}

TEST_F(EncodingEventSubscriberTest, FirstRtpTimeTicks) {
  Init(VIDEO_EVENT);
  RtpTimeTicks rtp_timestamp = RtpTimeTicks().Expand(UINT32_C(12345));
  base::TimeTicks now(testing_clock_.NowTicks());

  std::unique_ptr<FrameEvent> capture_begin_event(new FrameEvent());
  capture_begin_event->timestamp = now;
  capture_begin_event->type = FRAME_CAPTURE_BEGIN;
  capture_begin_event->media_type = VIDEO_EVENT;
  capture_begin_event->rtp_timestamp = rtp_timestamp;
  cast_environment_->logger()->DispatchFrameEvent(
      std::move(capture_begin_event));

  std::unique_ptr<FrameEvent> capture_end_event(new FrameEvent());
  capture_end_event->timestamp = now;
  capture_end_event->type = FRAME_CAPTURE_END;
  capture_end_event->media_type = VIDEO_EVENT;
  capture_end_event->rtp_timestamp =
      rtp_timestamp + RtpTimeDelta::FromTicks(30);
  capture_end_event->width = 1280;
  capture_end_event->height = 720;
  cast_environment_->logger()->DispatchFrameEvent(std::move(capture_end_event));

  GetEventsAndReset();

  EXPECT_EQ(rtp_timestamp, first_rtp_timestamp_);
  auto it = frame_events_.begin();
  ASSERT_NE(frame_events_.end(), it);
  EXPECT_EQ(0u, (*it)->relative_rtp_timestamp());

  ++it;
  ASSERT_NE(frame_events_.end(), it);
  EXPECT_EQ(30u, (*it)->relative_rtp_timestamp());
  EXPECT_EQ(1280, (*it)->width());
  EXPECT_EQ(720, (*it)->height());

  rtp_timestamp = rtp_timestamp.Expand(UINT32_C(67890));

  capture_begin_event.reset(new FrameEvent());
  capture_begin_event->timestamp = now;
  capture_begin_event->type = FRAME_CAPTURE_BEGIN;
  capture_begin_event->media_type = VIDEO_EVENT;
  capture_begin_event->rtp_timestamp = rtp_timestamp;
  cast_environment_->logger()->DispatchFrameEvent(
      std::move(capture_begin_event));

  GetEventsAndReset();

  EXPECT_EQ(rtp_timestamp, first_rtp_timestamp_);
}

TEST_F(EncodingEventSubscriberTest, RelativeRtpTimeTicksWrapAround) {
  Init(VIDEO_EVENT);
  RtpTimeTicks rtp_timestamp = RtpTimeTicks() - RtpTimeDelta::FromTicks(20);
  base::TimeTicks now(testing_clock_.NowTicks());

  std::unique_ptr<FrameEvent> capture_begin_event(new FrameEvent());
  capture_begin_event->timestamp = now;
  capture_begin_event->type = FRAME_CAPTURE_BEGIN;
  capture_begin_event->media_type = VIDEO_EVENT;
  capture_begin_event->rtp_timestamp = rtp_timestamp;
  cast_environment_->logger()->DispatchFrameEvent(
      std::move(capture_begin_event));

  // RtpTimeTicks has now wrapped around.
  std::unique_ptr<FrameEvent> capture_end_event(new FrameEvent());
  capture_end_event->timestamp = now;
  capture_end_event->type = FRAME_CAPTURE_END;
  capture_end_event->media_type = VIDEO_EVENT;
  capture_end_event->rtp_timestamp =
      rtp_timestamp + RtpTimeDelta::FromTicks(30);
  capture_end_event->width = 1280;
  capture_end_event->height = 720;
  cast_environment_->logger()->DispatchFrameEvent(std::move(capture_end_event));

  GetEventsAndReset();

  auto it = frame_events_.begin();
  ASSERT_NE(frame_events_.end(), it);
  EXPECT_EQ(0u, (*it)->relative_rtp_timestamp());

  ++it;
  ASSERT_NE(frame_events_.end(), it);
  EXPECT_EQ(30u, (*it)->relative_rtp_timestamp());
  EXPECT_EQ(1280, (*it)->width());
  EXPECT_EQ(720, (*it)->height());
}

TEST_F(EncodingEventSubscriberTest, MaxEventsPerProto) {
  Init(VIDEO_EVENT);
  RtpTimeTicks rtp_timestamp = RtpTimeTicks().Expand(UINT32_C(100));
  for (int i = 0; i < kMaxEventsPerProto + 1; i++) {
    std::unique_ptr<FrameEvent> ack_event(new FrameEvent());
    ack_event->timestamp = testing_clock_.NowTicks();
    ack_event->type = FRAME_ACK_RECEIVED;
    ack_event->media_type = VIDEO_EVENT;
    ack_event->rtp_timestamp = rtp_timestamp;
    ack_event->frame_id = FrameId::first();
    cast_environment_->logger()->DispatchFrameEvent(std::move(ack_event));

    task_runner_->Sleep(base::TimeDelta::FromMilliseconds(30));
  }

  GetEventsAndReset();

  ASSERT_EQ(2u, frame_events_.size());
  auto frame_it = frame_events_.begin();
  ASSERT_TRUE(frame_it != frame_events_.end());

  const AggregatedFrameEvent* frame_event = frame_it->get();

  EXPECT_EQ(kMaxEventsPerProto, frame_event->event_type_size());

  for (int i = 0; i < kMaxPacketsPerFrame + 1; i++) {
    std::unique_ptr<PacketEvent> send_event(new PacketEvent());
    send_event->timestamp = testing_clock_.NowTicks();
    send_event->type = PACKET_SENT_TO_NETWORK;
    send_event->media_type = VIDEO_EVENT;
    send_event->rtp_timestamp = rtp_timestamp;
    send_event->frame_id = FrameId::first();
    send_event->packet_id = i;
    send_event->max_packet_id = kMaxPacketsPerFrame;
    send_event->size = 123;
    cast_environment_->logger()->DispatchPacketEvent(std::move(send_event));

    task_runner_->Sleep(base::TimeDelta::FromMilliseconds(30));
  }

  GetEventsAndReset();

  EXPECT_EQ(2u, packet_events_.size());

  auto packet_it = packet_events_.begin();
  ASSERT_TRUE(packet_it != packet_events_.end());

  {
    const AggregatedPacketEvent* packet_event = packet_it->get();
    EXPECT_EQ(kMaxPacketsPerFrame, packet_event->base_packet_event_size());
  }

  ++packet_it;

  {
    const AggregatedPacketEvent* packet_event = packet_it->get();
    EXPECT_EQ(1, packet_event->base_packet_event_size());
  }

  for (int j = 0; j < kMaxEventsPerProto + 1; j++) {
    std::unique_ptr<PacketEvent> send_event(new PacketEvent());
    send_event->timestamp = testing_clock_.NowTicks();
    send_event->type = PACKET_SENT_TO_NETWORK;
    send_event->media_type = VIDEO_EVENT;
    send_event->rtp_timestamp = rtp_timestamp;
    send_event->frame_id = FrameId::first();
    send_event->packet_id = 0;
    send_event->max_packet_id = 0;
    send_event->size = 123;
    cast_environment_->logger()->DispatchPacketEvent(std::move(send_event));

    task_runner_->Sleep(base::TimeDelta::FromMilliseconds(30));
  }

  GetEventsAndReset();

  EXPECT_EQ(2u, packet_events_.size());
  packet_it = packet_events_.begin();
  ASSERT_TRUE(packet_it != packet_events_.end());

  {
    const AggregatedPacketEvent* packet_event = packet_it->get();
    EXPECT_EQ(kMaxEventsPerProto,
              packet_event->base_packet_event(0).event_type_size());
  }

  ++packet_it;
  {
    const AggregatedPacketEvent* packet_event = packet_it->get();
    EXPECT_EQ(1, packet_event->base_packet_event(0).event_type_size());
  }
}

}  // namespace cast
}  // namespace media
