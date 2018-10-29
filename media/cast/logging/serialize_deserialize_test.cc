// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Joint LogSerializer and LogDeserializer testing to make sure they stay in
// sync.

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "media/cast/logging/log_deserializer.h"
#include "media/cast/logging/log_serializer.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/logging/proto/proto_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using media::cast::proto::AggregatedFrameEvent;
using media::cast::proto::AggregatedPacketEvent;
using media::cast::proto::BasePacketEvent;
using media::cast::proto::LogMetadata;

namespace {

const media::cast::CastLoggingEvent kVideoFrameEvents[] = {
    media::cast::FRAME_CAPTURE_BEGIN,  media::cast::FRAME_CAPTURE_END,
    media::cast::FRAME_ENCODED, media::cast::FRAME_DECODED,
    media::cast::FRAME_PLAYOUT };

const media::cast::CastLoggingEvent kVideoPacketEvents[] = {
    media::cast::PACKET_SENT_TO_NETWORK, media::cast::PACKET_RECEIVED};

// The frame event fields cycle through these numbers.
const int kWidth[] = {1280, 1280, 1280, 1280, 1920, 1920, 1920, 1920};
const int kHeight[] = {720, 720, 720, 720, 1080, 1080, 1080, 1080};
const int kEncodedFrameSize[] = {512, 425, 399, 400, 237};
const int64_t kDelayMillis[] = {15, 4, 8, 42, 23, 16};
const int kEncoderCPUPercentUtilized[] = {10, 9, 42, 3, 11, 12, 15, 7};
const int kIdealizedBitratePercentUtilized[] = {9, 9, 9, 15, 36, 38, 35, 40};

const int kMaxSerializedBytes = 10000;

}  // namespace

namespace media {
namespace cast {

class SerializeDeserializeTest : public ::testing::Test {
 protected:
  SerializeDeserializeTest()
      : serialized_(new char[kMaxSerializedBytes]), output_bytes_(0) {}

  ~SerializeDeserializeTest() override = default;

  void Init() {
    metadata_.set_first_rtp_timestamp(12345678 * 90);
    metadata_.set_is_audio(false);
    metadata_.set_num_frame_events(10);
    metadata_.set_num_packet_events(10);

    int64_t event_time_ms = 0;
    // Insert frame and packet events with RTP timestamps 0, 90, 180, ...
    for (int i = 0; i < metadata_.num_frame_events(); i++) {
      auto frame_event = std::make_unique<AggregatedFrameEvent>();
      frame_event->set_relative_rtp_timestamp(i * 90);
      for (uint32_t event_index = 0; event_index < arraysize(kVideoFrameEvents);
           ++event_index) {
        frame_event->add_event_type(
            ToProtoEventType(kVideoFrameEvents[event_index]));
        frame_event->add_event_timestamp_ms(event_time_ms);
        event_time_ms += 1024;
      }
      frame_event->set_width(kWidth[i % arraysize(kWidth)]);
      frame_event->set_height(kHeight[i % arraysize(kHeight)]);
      frame_event->set_encoded_frame_size(
          kEncodedFrameSize[i % arraysize(kEncodedFrameSize)]);
      frame_event->set_delay_millis(kDelayMillis[i % arraysize(kDelayMillis)]);
      frame_event->set_encoder_cpu_percent_utilized(kEncoderCPUPercentUtilized[
              i % arraysize(kEncoderCPUPercentUtilized)]);
      frame_event->set_idealized_bitrate_percent_utilized(
          kIdealizedBitratePercentUtilized[
              i % arraysize(kIdealizedBitratePercentUtilized)]);

      frame_event_list_.push_back(std::move(frame_event));
    }

    event_time_ms = 0;
    int packet_id = 0;
    for (int i = 0; i < metadata_.num_packet_events(); i++) {
      auto packet_event = std::make_unique<AggregatedPacketEvent>();
      packet_event->set_relative_rtp_timestamp(i * 90);
      for (int j = 0; j < 10; j++) {
        BasePacketEvent* base_event = packet_event->add_base_packet_event();
        base_event->set_packet_id(packet_id);
        packet_id++;
        for (uint32_t event_index = 0;
             event_index < arraysize(kVideoPacketEvents); ++event_index) {
          base_event->add_event_type(
              ToProtoEventType(kVideoPacketEvents[event_index]));
          base_event->add_event_timestamp_ms(event_time_ms);
          event_time_ms += 256;
        }
      }
      packet_event_list_.push_back(std::move(packet_event));
    }
  }

  void Verify(const DeserializedLog& video_log) {
    const LogMetadata& returned_metadata = video_log.metadata;
    const FrameEventMap& returned_frame_events = video_log.frame_events;
    const PacketEventMap& returned_packet_events = video_log.packet_events;

    EXPECT_EQ(metadata_.SerializeAsString(),
              returned_metadata.SerializeAsString());

    // Check that the returned map is equal to the original map.
    EXPECT_EQ(frame_event_list_.size(), returned_frame_events.size());
    for (auto frame_it = returned_frame_events.begin();
         frame_it != returned_frame_events.end(); ++frame_it) {
      auto original_it = frame_event_list_.begin();
      ASSERT_NE(frame_event_list_.end(), original_it);
      // Compare protos by serializing and checking the bytes.
      EXPECT_EQ((*original_it)->SerializeAsString(),
                frame_it->second->SerializeAsString());
      frame_event_list_.erase(frame_event_list_.begin());
    }
    EXPECT_TRUE(frame_event_list_.empty());

    EXPECT_EQ(packet_event_list_.size(), returned_packet_events.size());
    for (auto packet_it = returned_packet_events.begin();
         packet_it != returned_packet_events.end(); ++packet_it) {
      auto original_it = packet_event_list_.begin();
      ASSERT_NE(packet_event_list_.end(), original_it);
      // Compare protos by serializing and checking the bytes.
      EXPECT_EQ((*original_it)->SerializeAsString(),
                packet_it->second->SerializeAsString());
      packet_event_list_.erase(packet_event_list_.begin());
    }
    EXPECT_TRUE(packet_event_list_.empty());
  }

  LogMetadata metadata_;
  FrameEventList frame_event_list_;
  PacketEventList packet_event_list_;
  std::unique_ptr<char[]> serialized_;
  int output_bytes_;
};

TEST_F(SerializeDeserializeTest, Uncompressed) {
  bool compressed = false;
  Init();

  bool success = SerializeEvents(metadata_,
                                 frame_event_list_,
                                 packet_event_list_,
                                 compressed,
                                 kMaxSerializedBytes,
                                 serialized_.get(),
                                 &output_bytes_);
  ASSERT_TRUE(success);
  ASSERT_GT(output_bytes_, 0);

  DeserializedLog audio_log;
  DeserializedLog video_log;
  success = DeserializeEvents(
      serialized_.get(), output_bytes_, compressed, &audio_log, &video_log);
  ASSERT_TRUE(success);

  Verify(video_log);
}

TEST_F(SerializeDeserializeTest, UncompressedInsufficientSpace) {
  bool compressed = false;
  Init();
  serialized_.reset(new char[100]);
  bool success = SerializeEvents(metadata_,
                                 frame_event_list_,
                                 packet_event_list_,
                                 compressed,
                                 100,
                                 serialized_.get(),
                                 &output_bytes_);
  EXPECT_FALSE(success);
  EXPECT_EQ(0, output_bytes_);
}

TEST_F(SerializeDeserializeTest, Compressed) {
  bool compressed = true;
  Init();
  bool success = SerializeEvents(metadata_,
                                 frame_event_list_,
                                 packet_event_list_,
                                 compressed,
                                 kMaxSerializedBytes,
                                 serialized_.get(),
                                 &output_bytes_);
  ASSERT_TRUE(success);
  ASSERT_GT(output_bytes_, 0);

  DeserializedLog audio_log;
  DeserializedLog video_log;
  success = DeserializeEvents(
      serialized_.get(), output_bytes_, compressed, &audio_log, &video_log);
  ASSERT_TRUE(success);
  Verify(video_log);
}

TEST_F(SerializeDeserializeTest, CompressedInsufficientSpace) {
  bool compressed = true;
  Init();
  serialized_.reset(new char[100]);
  bool success = SerializeEvents(metadata_,
                                 frame_event_list_,
                                 packet_event_list_,
                                 compressed,
                                 100,
                                 serialized_.get(),
                                 &output_bytes_);
  EXPECT_FALSE(success);
  EXPECT_EQ(0, output_bytes_);
}

}  // namespace cast
}  // namespace media
