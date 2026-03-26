// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mp4/hdr_metadata_track.h"

#include <vector>

#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "media/base/decoder_buffer_side_data.h"
#include "media/base/stream_parser.h"
#include "media/base/stream_parser_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/switches.h"

namespace media::mp4 {

namespace {
constexpr StreamParser::TrackId kMetadataTrackId = 1;
constexpr StreamParser::TrackId kRenderTrackId = 2;
}  // namespace

class HdrMetadataTrackTest : public testing::Test {
 public:
  HdrMetadataTrackTest()
      : metadata_track_(
            kMetadataTrackId,
            MetadataIT35SampleEntry::IT35PrefixType::kSmpteSt2094App5,
            {kRenderTrackId}) {
    feature_list_.InitWithFeatures({features::kHdrAgtm}, {});
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  HdrMetadataTrack metadata_track_;
};

TEST_F(HdrMetadataTrackTest, AttachMetadata) {
  StreamParser::BufferQueueMap buffers;

  // Create a metadata buffer.
  uint8_t metadata_data[] = {0x00, 0x80, 0x07, 0xd0};
  auto metadata_buffer = StreamParserBuffer::CopyFrom(
      metadata_data, true, DemuxerStream::Type::UNKNOWN, kMetadataTrackId);
  metadata_buffer->set_timestamp(base::Seconds(1));
  metadata_buffer->set_duration(base::Seconds(1));
  buffers[kMetadataTrackId].push_back(metadata_buffer);

  // Create a render buffer at the same timestamp.
  uint8_t render_data[] = {0x04, 0x05};
  auto render_buffer = StreamParserBuffer::CopyFrom(
      render_data, true, DemuxerStream::Type::VIDEO, kRenderTrackId);
  render_buffer->set_timestamp(base::Seconds(1));
  render_buffer->set_duration(base::Seconds(1));
  buffers[kRenderTrackId].push_back(render_buffer);

  metadata_track_.AttachMetadataOrHoldBuffers(&buffers, false);

  // Metadata track should be removed from buffers.
  EXPECT_EQ(buffers.count(kMetadataTrackId), 0u);

  // Render track should still be there.
  ASSERT_EQ(buffers.count(kRenderTrackId), 1u);
  ASSERT_EQ(buffers[kRenderTrackId].size(), 1u);

  // Metadata should be attached to the render buffer.
  auto& buffer = buffers[kRenderTrackId][0];
  ASSERT_TRUE(buffer->side_data());
  ASSERT_TRUE(buffer->side_data()->hdr_metadata.HasAgtm());
  EXPECT_EQ(buffer->side_data()->hdr_metadata.GetAgtm().fHdrReferenceWhite,
            400.f);
}

TEST_F(HdrMetadataTrackTest, HoldBuffers) {
  StreamParser::BufferQueueMap buffers;

  // Create a render buffer at a timestamp for which we don't have metadata.
  uint8_t render_data[] = {0x04, 0x05};
  auto render_buffer = StreamParserBuffer::CopyFrom(
      render_data, true, DemuxerStream::Type::VIDEO, kRenderTrackId);
  render_buffer->set_timestamp(base::Seconds(1));
  render_buffer->set_duration(base::Seconds(1));
  buffers[kRenderTrackId].push_back(render_buffer);

  metadata_track_.AttachMetadataOrHoldBuffers(&buffers, false);

  // Render track should be removed from buffers because it's being held.
  EXPECT_EQ(buffers.count(kRenderTrackId), 0u);

  // Now add metadata and call again.
  uint8_t metadata_data[] = {0x00, 0x80, 0x03, 0xe8};
  auto metadata_buffer = StreamParserBuffer::CopyFrom(
      metadata_data, true, DemuxerStream::Type::UNKNOWN, kMetadataTrackId);
  metadata_buffer->set_timestamp(base::Seconds(1));
  metadata_buffer->set_duration(base::Seconds(1));
  buffers[kMetadataTrackId].push_back(metadata_buffer);

  metadata_track_.AttachMetadataOrHoldBuffers(&buffers, false);

  // Render track should now be returned to buffers.
  ASSERT_EQ(buffers.count(kRenderTrackId), 1u);
  ASSERT_EQ(buffers[kRenderTrackId].size(), 1u);

  // Metadata should be attached.
  auto& buffer = buffers[kRenderTrackId][0];
  ASSERT_TRUE(buffer->side_data());
  ASSERT_TRUE(buffer->side_data()->hdr_metadata.HasAgtm());
  EXPECT_EQ(buffer->side_data()->hdr_metadata.GetAgtm().fHdrReferenceWhite,
            200.f);
}

TEST_F(HdrMetadataTrackTest, AllSamplesReceived) {
  StreamParser::BufferQueueMap buffers;

  // Create a render buffer at a timestamp for which we don't have metadata.
  uint8_t render_data[] = {0x04, 0x05};
  auto render_buffer = StreamParserBuffer::CopyFrom(
      render_data, true, DemuxerStream::Type::VIDEO, kRenderTrackId);
  render_buffer->set_timestamp(base::Seconds(1));
  render_buffer->set_duration(base::Seconds(1));
  buffers[kRenderTrackId].push_back(render_buffer);

  // Call with all_samples_received = true.
  metadata_track_.AttachMetadataOrHoldBuffers(&buffers, true);

  // Render track should be returned even without metadata.
  ASSERT_EQ(buffers.count(kRenderTrackId), 1u);
  ASSERT_EQ(buffers[kRenderTrackId].size(), 1u);
  EXPECT_FALSE(buffers[kRenderTrackId][0]->side_data());
}

TEST_F(HdrMetadataTrackTest, OrderVerification) {
  // Create Buffer 1 (PTS=2, DTS=1).
  uint8_t data1[] = {0x01};
  auto buffer1 = StreamParserBuffer::CopyFrom(
      data1, true, DemuxerStream::Type::VIDEO, kRenderTrackId);
  buffer1->set_timestamp(base::Seconds(2));
  buffer1->SetDecodeTimestamp(DecodeTimestamp::FromSecondsD(1));
  buffer1->set_duration(base::Seconds(1));

  // Create Buffer 2 (PTS=1, DTS=2).
  uint8_t data2[] = {0x02};
  auto buffer2 = StreamParserBuffer::CopyFrom(
      data2, true, DemuxerStream::Type::VIDEO, kRenderTrackId);
  buffer2->set_timestamp(base::Seconds(1));
  buffer2->SetDecodeTimestamp(DecodeTimestamp::FromSecondsD(2));
  buffer2->set_duration(base::Seconds(1));

  // Create Buffer 3 (PTS=3, DTS=3).
  uint8_t data3[] = {0x03};
  auto buffer3 = StreamParserBuffer::CopyFrom(
      data3, true, DemuxerStream::Type::VIDEO, kRenderTrackId);
  buffer3->set_timestamp(base::Seconds(3));
  buffer3->SetDecodeTimestamp(DecodeTimestamp::FromSecondsD(3));
  buffer3->set_duration(base::Seconds(1));

  // Create metadata buffer (PTS=0, duration=5).
  uint8_t metadata_data[] = {0x00, 0x80, 0x1a, 0x1d};
  auto mbuf = StreamParserBuffer::CopyFrom(
      metadata_data, true, DemuxerStream::Type::UNKNOWN, kMetadataTrackId);
  mbuf->set_timestamp(base::Seconds(0));
  mbuf->SetDecodeTimestamp(DecodeTimestamp::FromSecondsD(0));
  mbuf->set_duration(base::Seconds(5));

  StreamParser::BufferQueueMap buffers;

  // Send buffers 1 and 2. No metadata yet.
  buffers[kRenderTrackId].push_back(buffer1);
  buffers[kRenderTrackId].push_back(buffer2);
  metadata_track_.AttachMetadataOrHoldBuffers(&buffers, false);
  EXPECT_TRUE(buffers.empty());

  // Send buffer 3 and the metadata.
  buffers[kRenderTrackId].push_back(buffer3);
  buffers[kMetadataTrackId].push_back(mbuf);
  metadata_track_.AttachMetadataOrHoldBuffers(&buffers, false);

  // Verify all 3 buffers are returned in original order (DTS order).
  ASSERT_EQ(buffers.count(kRenderTrackId), 1u);
  const auto& buffer_queue = buffers[kRenderTrackId];
  ASSERT_EQ(buffer_queue.size(), 3u);
  EXPECT_EQ(buffer_queue[0], buffer1);
  EXPECT_EQ(buffer_queue[1], buffer2);
  EXPECT_EQ(buffer_queue[2], buffer3);
  for (const auto& buffer : buffer_queue) {
    ASSERT_TRUE(buffer->side_data());
    ASSERT_TRUE(buffer->side_data()->hdr_metadata.HasAgtm());
    EXPECT_EQ(buffer->side_data()->hdr_metadata.GetAgtm().fHdrReferenceWhite,
              1337.f);
  }
}

}  // namespace media::mp4
