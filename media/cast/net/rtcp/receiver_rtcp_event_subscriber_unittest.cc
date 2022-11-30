// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/net/rtcp/receiver_rtcp_event_subscriber.h"

#include <stddef.h>
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

namespace media {
namespace cast {

namespace {

const size_t kMaxEventEntries = 10u;
const int64_t kDelayMs = 20L;

}  // namespace

class ReceiverRtcpEventSubscriberTest : public ::testing::Test {
 protected:
  ReceiverRtcpEventSubscriberTest()
      : task_runner_(new FakeSingleThreadTaskRunner(&testing_clock_)),
        cast_environment_(new CastEnvironment(&testing_clock_,
                                              task_runner_,
                                              task_runner_,
                                              task_runner_)) {}

  ~ReceiverRtcpEventSubscriberTest() override = default;

  void TearDown() final {
    if (event_subscriber_) {
      cast_environment_->logger()->Unsubscribe(event_subscriber_.get());
      event_subscriber_.reset();
    }
  }

  void Init(EventMediaType type) {
    event_subscriber_ =
        std::make_unique<ReceiverRtcpEventSubscriber>(kMaxEventEntries, type);
    cast_environment_->logger()->Subscribe(event_subscriber_.get());
  }

  void InsertEvents() {
    // Video events
    std::unique_ptr<FrameEvent> playout_event(new FrameEvent());
    playout_event->timestamp = testing_clock_.NowTicks();
    playout_event->type = FRAME_PLAYOUT;
    playout_event->media_type = VIDEO_EVENT;
    playout_event->rtp_timestamp = RtpTimeTicks().Expand(UINT32_C(100));
    playout_event->frame_id = FrameId::first() + 2;
    playout_event->delay_delta = base::Milliseconds(kDelayMs);
    cast_environment_->logger()->DispatchFrameEvent(std::move(playout_event));

    std::unique_ptr<FrameEvent> decode_event(new FrameEvent());
    decode_event->timestamp = testing_clock_.NowTicks();
    decode_event->type = FRAME_DECODED;
    decode_event->media_type = VIDEO_EVENT;
    decode_event->rtp_timestamp = RtpTimeTicks().Expand(UINT32_C(200));
    decode_event->frame_id = FrameId::first() + 1;
    cast_environment_->logger()->DispatchFrameEvent(std::move(decode_event));

    std::unique_ptr<PacketEvent> receive_event(new PacketEvent());
    receive_event->timestamp = testing_clock_.NowTicks();
    receive_event->type = PACKET_RECEIVED;
    receive_event->media_type = VIDEO_EVENT;
    receive_event->rtp_timestamp = RtpTimeTicks().Expand(UINT32_C(200));
    receive_event->frame_id = FrameId::first() + 2;
    receive_event->packet_id = 1u;
    receive_event->max_packet_id = 10u;
    receive_event->size = 1024u;
    cast_environment_->logger()->DispatchPacketEvent(std::move(receive_event));

    // Audio events
    playout_event = std::make_unique<FrameEvent>();
    playout_event->timestamp = testing_clock_.NowTicks();
    playout_event->type = FRAME_PLAYOUT;
    playout_event->media_type = AUDIO_EVENT;
    playout_event->rtp_timestamp = RtpTimeTicks().Expand(UINT32_C(300));
    playout_event->frame_id = FrameId::first() + 4;
    playout_event->delay_delta = base::Milliseconds(kDelayMs);
    cast_environment_->logger()->DispatchFrameEvent(std::move(playout_event));

    decode_event = std::make_unique<FrameEvent>();
    decode_event->timestamp = testing_clock_.NowTicks();
    decode_event->type = FRAME_DECODED;
    decode_event->media_type = AUDIO_EVENT;
    decode_event->rtp_timestamp = RtpTimeTicks().Expand(UINT32_C(400));
    decode_event->frame_id = FrameId::first() + 3;
    cast_environment_->logger()->DispatchFrameEvent(std::move(decode_event));

    receive_event = std::make_unique<PacketEvent>();
    receive_event->timestamp = testing_clock_.NowTicks();
    receive_event->type = PACKET_RECEIVED;
    receive_event->media_type = AUDIO_EVENT;
    receive_event->rtp_timestamp = RtpTimeTicks().Expand(UINT32_C(400));
    receive_event->frame_id = FrameId::first() + 5;
    receive_event->packet_id = 1u;
    receive_event->max_packet_id = 10u;
    receive_event->size = 128u;
    cast_environment_->logger()->DispatchPacketEvent(std::move(receive_event));

    // Unrelated events
    std::unique_ptr<FrameEvent> encode_event(new FrameEvent());
    encode_event->timestamp = testing_clock_.NowTicks();
    encode_event->type = FRAME_ENCODED;
    encode_event->media_type = VIDEO_EVENT;
    encode_event->rtp_timestamp = RtpTimeTicks().Expand(UINT32_C(100));
    encode_event->frame_id = FrameId::first() + 1;
    cast_environment_->logger()->DispatchFrameEvent(std::move(encode_event));

    encode_event = std::make_unique<FrameEvent>();
    encode_event->timestamp = testing_clock_.NowTicks();
    encode_event->type = FRAME_ENCODED;
    encode_event->media_type = AUDIO_EVENT;
    encode_event->rtp_timestamp = RtpTimeTicks().Expand(UINT32_C(100));
    encode_event->frame_id = FrameId::first() + 1;
    cast_environment_->logger()->DispatchFrameEvent(std::move(encode_event));
  }

  base::SimpleTestTickClock testing_clock_;
  scoped_refptr<FakeSingleThreadTaskRunner> task_runner_;
  scoped_refptr<CastEnvironment> cast_environment_;
  std::unique_ptr<ReceiverRtcpEventSubscriber> event_subscriber_;
};

TEST_F(ReceiverRtcpEventSubscriberTest, LogVideoEvents) {
  Init(VIDEO_EVENT);

  InsertEvents();
  ReceiverRtcpEventSubscriber::RtcpEvents rtcp_events;
  event_subscriber_->GetRtcpEventsWithRedundancy(&rtcp_events);
  EXPECT_EQ(3u, rtcp_events.size());
}

TEST_F(ReceiverRtcpEventSubscriberTest, LogAudioEvents) {
  Init(AUDIO_EVENT);

  InsertEvents();
  ReceiverRtcpEventSubscriber::RtcpEvents rtcp_events;
  event_subscriber_->GetRtcpEventsWithRedundancy(&rtcp_events);
  EXPECT_EQ(3u, rtcp_events.size());
}

TEST_F(ReceiverRtcpEventSubscriberTest, DropEventsWhenSizeExceeded) {
  Init(VIDEO_EVENT);

  for (int i = 1; i <= 10; ++i) {
    std::unique_ptr<FrameEvent> decode_event(new FrameEvent());
    decode_event->timestamp = testing_clock_.NowTicks();
    decode_event->type = FRAME_DECODED;
    decode_event->media_type = VIDEO_EVENT;
    decode_event->rtp_timestamp =
        RtpTimeTicks().Expand(static_cast<unsigned int>(i * 10));
    decode_event->frame_id = FrameId::first() + i;
    cast_environment_->logger()->DispatchFrameEvent(std::move(decode_event));
  }

  ReceiverRtcpEventSubscriber::RtcpEvents rtcp_events;
  event_subscriber_->GetRtcpEventsWithRedundancy(&rtcp_events);
  EXPECT_EQ(10u, rtcp_events.size());
}

}  // namespace cast
}  // namespace media
