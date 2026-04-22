// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/hdr_metadata_reordering_map.h"

#include "media/base/decoder_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(HdrMetadataReorderingMapTest, InsertAndMerge) {
  HdrMetadataReorderingMap map;
  gfx::HDRMetadata metadata;
  skhdr::MasteringDisplayColorVolume mdcv;
  mdcv.fMaximumDisplayMasteringLuminance = 1000.0f;
  metadata.SetMDCV(mdcv);

  base::TimeDelta timestamp = base::Microseconds(100);
  auto buffer = DecoderBuffer::CopyFrom(base::span<const uint8_t>());
  buffer->set_timestamp(timestamp);
  buffer->WritableSideData().hdr_metadata = metadata;

  map.Insert(*buffer);

  gfx::HDRMetadata target_metadata;
  map.MergeAndEraseMetadataForTimestamp(timestamp, target_metadata);
  ASSERT_TRUE(target_metadata.HasMDCV());
  EXPECT_EQ(target_metadata.GetMDCV().fMaximumDisplayMasteringLuminance,
            1000.0f);

  // Second merge should do nothing as it was erased.
  gfx::HDRMetadata target_metadata2;
  map.MergeAndEraseMetadataForTimestamp(timestamp, target_metadata2);
  EXPECT_FALSE(target_metadata2.HasMDCV());
}

TEST(HdrMetadataReorderingMapTest, MergeAndEraseMultiple) {
  HdrMetadataReorderingMap map;

  auto b1 = DecoderBuffer::CopyFrom(base::span<const uint8_t>());
  b1->set_timestamp(base::Microseconds(100));
  b1->WritableSideData().hdr_metadata.SetMDCV(
      {.fMaximumDisplayMasteringLuminance = 100.0f});

  auto b2 = DecoderBuffer::CopyFrom(base::span<const uint8_t>());
  b2->set_timestamp(base::Microseconds(200));
  b2->WritableSideData().hdr_metadata.SetMDCV(
      {.fMaximumDisplayMasteringLuminance = 200.0f});

  auto b3 = DecoderBuffer::CopyFrom(base::span<const uint8_t>());
  b3->set_timestamp(base::Microseconds(300));
  b3->WritableSideData().hdr_metadata.SetMDCV(
      {.fMaximumDisplayMasteringLuminance = 300.0f});

  map.Insert(*b1);
  map.Insert(*b2);
  map.Insert(*b3);

  // Merging with 200 should return m2 and erase 100 and 200.
  gfx::HDRMetadata target;
  map.MergeAndEraseMetadataForTimestamp(base::Microseconds(200), target);
  ASSERT_TRUE(target.HasMDCV());
  EXPECT_EQ(target.GetMDCV().fMaximumDisplayMasteringLuminance, 200.0f);

  // 100 should be gone.
  gfx::HDRMetadata target100;
  map.MergeAndEraseMetadataForTimestamp(base::Microseconds(100), target100);
  EXPECT_FALSE(target100.HasMDCV());

  // 300 should still be there.
  gfx::HDRMetadata target300;
  map.MergeAndEraseMetadataForTimestamp(base::Microseconds(300), target300);
  ASSERT_TRUE(target300.HasMDCV());
  EXPECT_EQ(target300.GetMDCV().fMaximumDisplayMasteringLuminance, 300.0f);
}

TEST(HdrMetadataReorderingMapTest, Clear) {
  HdrMetadataReorderingMap map;
  auto b = DecoderBuffer::CopyFrom(base::span<const uint8_t>());
  b->set_timestamp(base::Microseconds(100));
  b->WritableSideData().hdr_metadata.SetMDCV(
      {.fMaximumDisplayMasteringLuminance = 100.0f});

  map.Insert(*b);
  map.Clear();
  gfx::HDRMetadata target;
  map.MergeAndEraseMetadataForTimestamp(base::Microseconds(100), target);
  EXPECT_TRUE(target.IsEmpty());
}

}  // namespace media
