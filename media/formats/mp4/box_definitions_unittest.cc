// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mp4/box_definitions.h"

#include <stdint.h>

#include <array>
#include <memory>
#include <vector>

#include "media/base/mock_media_log.h"
#include "media/formats/mp4/box_reader.h"
#include "media/formats/mp4/parse_result.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media::mp4 {

using testing::StrictMock;

class BoxDefinitionsTest : public testing::Test {
 public:
  BoxDefinitionsTest() = default;

 protected:
  StrictMock<MockMediaLog> media_log_;
};

TEST_F(BoxDefinitionsTest, Stereoscopic3DBoxParsing) {
  // Box size = 13 bytes (0x0d), FourCC = 'st3d', Full box version = 0, flags =
  // 0, stereo_mode = 2 (Side-by-Side Left First)
  static constexpr auto kData = std::to_array<uint8_t>({
      0x00, 0x00, 0x00, 0x0d,  // size
      's', 't', '3', 'd',      // type
      0x00, 0x00, 0x00, 0x00,  // version and flags
      0x02                     // stereo_mode (Side-by-Side Left First)
  });

  std::unique_ptr<BoxReader> reader(
      BoxReader::ReadConcatentatedBoxes(kData, &media_log_));
  ASSERT_TRUE(reader->ScanChildren());

  Stereoscopic3DVideo st3d;
  EXPECT_TRUE(reader->ReadChild(&st3d));
  EXPECT_EQ(st3d.mode, VideoStereoMode::kSideBySideLeftFirst);
}

TEST_F(BoxDefinitionsTest, SphericalVideoEquirectangularParsing) {
  // Construct a valid nested 'sv3d' Equirectangular payload (81 bytes total
  // size)
  static constexpr auto kData = std::to_array<uint8_t>({
      // --- sv3d Box (Spherical Video V2) ---
      0x00, 0x00, 0x00, 0x51,  // size = 81 bytes (0x51)
      's', 'v', '3', 'd',      // type

      // --- svhd Box (Spherical Video Header) ---
      0x00, 0x00, 0x00, 0x0d,  // size = 13 bytes (0x0d)
      's', 'v', 'h', 'd',      // type
      0x00, 0x00, 0x00, 0x00,  // version and flags
      0x00,  // metadata_source (empty string terminated by \0)

      // --- proj Box (Projection) ---
      0x00, 0x00, 0x00, 0x3c,  // size = 60 bytes (0x3c)
      'p', 'r', 'o', 'j',      // type

      // --- prhd Box (Projection Header) ---
      0x00, 0x00, 0x00, 0x18,  // size = 24 bytes (0x18)
      'p', 'r', 'h', 'd',      // type
      0x00, 0x00, 0x00, 0x00,  // version and flags
      0x00, 0x0a, 0x00, 0x00,  // pose_yaw = 10.0
      0x00, 0x14, 0x00, 0x00,  // pose_pitch = 20.0
      0x00, 0x1e, 0x00, 0x00,  // pose_roll = 30.0

      // --- equi Box (Equirectangular) ---
      0x00, 0x00, 0x00, 0x1c,  // size = 28 bytes (0x1c)
      'e', 'q', 'u', 'i',      // type
      0x00, 0x00, 0x00, 0x00,  // version and flags
      0x00, 0x00, 0x00, 0x0a,  // bounds_top = 10
      0x00, 0x00, 0x00, 0x14,  // bounds_bottom = 20
      0x00, 0x00, 0x00, 0x1e,  // bounds_left = 30
      0x00, 0x00, 0x00, 0x28   // bounds_right = 40
  });

  std::unique_ptr<BoxReader> reader(
      BoxReader::ReadConcatentatedBoxes(kData, &media_log_));
  ASSERT_TRUE(reader->ScanChildren());

  SphericalVideo sv3d;
  EXPECT_TRUE(reader->ReadChild(&sv3d));

  EXPECT_EQ(sv3d.projection.type, VideoProjectionType::kEquirect360);
}

}  // namespace media::mp4
