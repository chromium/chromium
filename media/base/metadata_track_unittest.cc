// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/metadata_track.h"

#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decoder_buffer_side_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/switches.h"

namespace media {

namespace {

// AGTM payloads with an fHdrReferenceWhite of 400 and 200 nits.
constexpr uint8_t kMetadata400Nits[] = {0x00, 0x80, 0x07, 0xd0};
constexpr uint8_t kMetadata200Nits[] = {0x00, 0x80, 0x03, 0xe8};
constexpr uint8_t kRenderData[] = {0x04, 0x05};

scoped_refptr<DecoderBuffer> MakeBuffer(base::span<const uint8_t> data,
                                        base::TimeDelta timestamp,
                                        base::TimeDelta duration) {
  auto buffer = DecoderBuffer::CopyFrom(data);
  buffer->set_timestamp(timestamp);
  buffer->set_duration(duration);
  return buffer;
}

float GetAttachedReferenceWhite(const DecoderBuffer& buffer) {
  CHECK(buffer.side_data());
  CHECK(buffer.side_data()->hdr_metadata.HasAgtm());
  return buffer.side_data()->hdr_metadata.GetAgtm().fHdrReferenceWhite;
}

}  // namespace

class MetadataTrackTest : public testing::Test {
 public:
  MetadataTrackTest()
      : metadata_track_(MetadataTrack::IT35PrefixType::kSmpteSt2094App5) {
    feature_list_.InitWithFeatures({features::kHdrAgtm}, {});
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  MetadataTrack metadata_track_;
};

TEST_F(MetadataTrackTest, AttachMetadata) {
  metadata_track_.InsertMetadataBuffer(
      *MakeBuffer(kMetadata400Nits, base::Seconds(1), base::Seconds(1)));

  auto render_buffer =
      MakeBuffer(kRenderData, base::Seconds(1), base::Seconds(1));

  EXPECT_TRUE(metadata_track_.TryAttachMetadata(*render_buffer));
  EXPECT_EQ(GetAttachedReferenceWhite(*render_buffer), 400.f);
}

TEST_F(MetadataTrackTest, NoMetadataForTimestamp) {
  metadata_track_.InsertMetadataBuffer(
      *MakeBuffer(kMetadata400Nits, base::Seconds(1), base::Seconds(1)));

  // The metadata above covers [1s, 2s). This buffer is outside of it, so the
  // caller is told to keep waiting, and the buffer is left untouched.
  auto render_buffer =
      MakeBuffer(kRenderData, base::Seconds(3), base::Seconds(1));

  EXPECT_FALSE(metadata_track_.TryAttachMetadata(*render_buffer));
  EXPECT_FALSE(render_buffer->side_data());
}

TEST_F(MetadataTrackTest, MetadataArrivesAfterBuffer) {
  auto render_buffer =
      MakeBuffer(kRenderData, base::Seconds(1), base::Seconds(1));

  EXPECT_FALSE(metadata_track_.TryAttachMetadata(*render_buffer));

  metadata_track_.InsertMetadataBuffer(
      *MakeBuffer(kMetadata200Nits, base::Seconds(1), base::Seconds(1)));

  EXPECT_TRUE(metadata_track_.TryAttachMetadata(*render_buffer));
  EXPECT_EQ(GetAttachedReferenceWhite(*render_buffer), 200.f);
}

TEST_F(MetadataTrackTest, RetainsRecentMetadataForReorderedBuffers) {
  metadata_track_.InsertMetadataBuffer(
      *MakeBuffer(kMetadata200Nits, base::Seconds(10), base::Seconds(1)));
  metadata_track_.InsertMetadataBuffer(
      *MakeBuffer(kMetadata400Nits, base::Seconds(11), base::Seconds(1)));

  // Buffers are consumed in decode order, so a buffer may be presented before
  // one that was already consumed. Metadata just behind the last consumed
  // timestamp is retained for them.
  auto later_buffer =
      MakeBuffer(kRenderData, base::Seconds(11), base::Seconds(1));
  EXPECT_TRUE(metadata_track_.TryAttachMetadata(*later_buffer));
  EXPECT_EQ(GetAttachedReferenceWhite(*later_buffer), 400.f);

  auto earlier_buffer =
      MakeBuffer(kRenderData, base::Seconds(10), base::Seconds(1));
  EXPECT_TRUE(metadata_track_.TryAttachMetadata(*earlier_buffer));
  EXPECT_EQ(GetAttachedReferenceWhite(*earlier_buffer), 200.f);
}

TEST_F(MetadataTrackTest, Reset) {
  metadata_track_.InsertMetadataBuffer(
      *MakeBuffer(kMetadata400Nits, base::Seconds(1), base::Seconds(1)));

  metadata_track_.Reset();

  auto render_buffer =
      MakeBuffer(kRenderData, base::Seconds(1), base::Seconds(1));
  EXPECT_FALSE(metadata_track_.TryAttachMetadata(*render_buffer));
  EXPECT_FALSE(render_buffer->side_data());
}

}  // namespace media
