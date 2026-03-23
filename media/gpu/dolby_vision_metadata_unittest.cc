// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/dolby_vision_metadata.h"

#include "base/time/time.h"
#include "media/media_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(DolbyVisionMetadataTest, FromRawAndFromH265) {
  // Raw metadata should be preserved as-is.
  constexpr uint8_t kFirstInput[] = {0x01, 0x02, 0x03};
  constexpr uint8_t kFirstExpected[] = {0x01, 0x02, 0x03};
  // H.265 metadata should drop emulation prevention bytes when applicable.
  constexpr uint8_t kSecondInput[] = {0x09, 0x00, 0x00, 0x03, 0x02, 0x0A};
  constexpr uint8_t kSecondExpected[] = {0x09, 0x00, 0x00, 0x02, 0x0A};
  // Literal 0x03 must be preserved when it is not an emulation prevention byte.
  constexpr uint8_t kThirdInput[] = {0x09, 0x00, 0x00, 0x03, 0x7C, 0x0A};
  constexpr uint8_t kThirdExpected[] = {0x09, 0x00, 0x00, 0x03, 0x7C, 0x0A};

  const auto raw_metadata =
      DolbyVisionMetadata::FromRaw(kFirstInput, base::Milliseconds(5));
  const auto h265_metadata =
      DolbyVisionMetadata::FromH265(kSecondInput, base::Milliseconds(7));
  const auto h265_literal_metadata =
      DolbyVisionMetadata::FromH265(kThirdInput, base::Milliseconds(9));

  EXPECT_EQ(base::span<const uint8_t>(raw_metadata.data),
            base::span<const uint8_t>(kFirstExpected));
  EXPECT_EQ(raw_metadata.timestamp, base::Milliseconds(5));
  EXPECT_EQ(base::span<const uint8_t>(h265_metadata.data),
            base::span<const uint8_t>(kSecondExpected));
  EXPECT_EQ(h265_metadata.timestamp, base::Milliseconds(7));
  EXPECT_EQ(base::span<const uint8_t>(h265_literal_metadata.data),
            base::span<const uint8_t>(kThirdExpected));
  EXPECT_EQ(h265_literal_metadata.timestamp, base::Milliseconds(9));
}

}  // namespace media
