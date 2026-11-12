// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/logging.h"
#include "base/numerics/byte_conversions.h"
#include "media/base/media_util.h"
#include "media/base/video_codecs.h"
#include "media/formats/mp4/box_definitions.h"
#include "media/formats/mp4/box_reader.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace media::mp4 {
namespace {

struct Environment {
  Environment() { logging::SetMinLogLevel(logging::LOGGING_FATAL); }
};

[[maybe_unused]] const Environment kEnv;

// Valid 28-byte OutOfBandAV2CodecConfigurationBox ('av2C') payload (Main
// 4:2:0 10-bit IP0, level 2.1, 8-bit) used as an initial fuzzing seed.
constexpr uint8_t kValidOutOfBandAv2CPayload[] = {
    0x00, 0x00, 0x19, 0x04, 0x80, 0x1a, 0x00, 0x22, 0x6d, 0xf4,
    0x78, 0x3c, 0xc0, 0x00, 0x00, 0x1c, 0xdd, 0xcd, 0x01, 0x35,
    0x7e, 0x77, 0x7e, 0xa4, 0xdc, 0xca, 0xd0, 0x80,
};

// Multi-OBU 'av2C' seed containing an Operating Point Set OBU followed by a
// Sequence Header OBU with an extension header (Main 4:2:2 10-bit IP1).
constexpr uint8_t kMultiObuOutOfBandAv2CPayload[] = {
    0x00, 0x01, 0x02, 0x48, 0x80, 0x05, 0x84, 0x01, 0x8c, 0x3d, 0x40,
};

// Valid 2-byte InBandAV2CodecConfigurationBox ('av2c') payload (Main 4:2:2
// 10-bit IP1, level_idx = 10, tier = 0).
constexpr uint8_t kValidInBandAv2cPayload[] = {0x00, 0xd4};

std::vector<uint8_t> WrapMp4Box(std::string_view fourcc,
                                base::span<const uint8_t> payload) {
  std::vector<uint8_t> out;
  out.reserve(8 + payload.size());
  const auto size_bytes =
      base::U32ToBigEndian(static_cast<uint32_t>(8 + payload.size()));
  out.insert(out.end(), size_bytes.begin(), size_bytes.end());
  out.insert(out.end(), fourcc.begin(), fourcc.end());
  out.insert(out.end(), payload.begin(), payload.end());
  return out;
}

std::vector<uint8_t> BuildVisualSampleEntryBox(
    std::string_view fourcc,
    uint16_t width,
    uint16_t height,
    base::span<const uint8_t> child_boxes) {
  std::vector<uint8_t> payload(78, 0);
  payload[7] = 1;  // data_reference_index = 1
  const auto w_bytes = base::U16ToBigEndian(width);
  const auto h_bytes = base::U16ToBigEndian(height);
  payload[24] = w_bytes[0];
  payload[25] = w_bytes[1];
  payload[26] = h_bytes[0];
  payload[27] = h_bytes[1];
  payload.insert(payload.end(), child_boxes.begin(), child_boxes.end());
  return WrapMp4Box(fourcc, payload);
}

void ParseOutOfBandConfig(const std::vector<uint8_t>& data) {
  AV2CodecConfigurationRecord direct_config;
  const bool direct_ok = direct_config.Parse(data);
  if (direct_ok) {
    ASSERT_GE(data.size(), 4u);
    EXPECT_GE(direct_config.profile, AV2PROFILE_MIN);
    EXPECT_LE(direct_config.profile, AV2PROFILE_MAX);
    EXPECT_LE(direct_config.level, 31u);
    EXPECT_EQ(base::span(direct_config.config_obus),
              base::span(data).subspan(2u));
  }

  if (data.size() <=
      static_cast<size_t>(std::numeric_limits<int32_t>::max()) - 8u) {
    const std::vector<uint8_t> box_bytes = WrapMp4Box("av2C", data);
    NullMediaLog media_log;
    std::unique_ptr<BoxReader> reader(
        BoxReader::ReadConcatentatedBoxes(box_bytes, &media_log));
    ASSERT_TRUE(reader->ScanChildren());

    AV2CodecConfigurationRecord box_config;
    EXPECT_EQ(reader->ReadChild(&box_config), direct_ok);
    if (direct_ok) {
      EXPECT_EQ(box_config.profile, direct_config.profile);
      EXPECT_EQ(box_config.level, direct_config.level);
      EXPECT_EQ(box_config.config_obus, direct_config.config_obus);
    }
  }
}

FUZZ_TEST(Mp4Av2BoxFuzzTest, ParseOutOfBandConfig)
    .WithDomains(fuzztest::Arbitrary<std::vector<uint8_t>>())
    .WithSeeds({
        {std::vector<uint8_t>(std::begin(kValidOutOfBandAv2CPayload),
                              std::end(kValidOutOfBandAv2CPayload))},
        {std::vector<uint8_t>(std::begin(kMultiObuOutOfBandAv2CPayload),
                              std::end(kMultiObuOutOfBandAv2CPayload))},
    });

void ParseInBandConfig(const std::vector<uint8_t>& data) {
  InBandAV2CodecConfigurationRecord direct_config;
  const bool direct_ok = direct_config.Parse(data);
  if (direct_ok) {
    ASSERT_GE(data.size(), 2u);
    EXPECT_GE(direct_config.profile, AV2PROFILE_MIN);
    EXPECT_LE(direct_config.profile, AV2PROFILE_MAX);
    EXPECT_LE(direct_config.level, 31u);
  }

  if (data.size() <=
      static_cast<size_t>(std::numeric_limits<int32_t>::max()) - 8u) {
    const std::vector<uint8_t> box_bytes = WrapMp4Box("av2c", data);
    NullMediaLog media_log;
    std::unique_ptr<BoxReader> reader(
        BoxReader::ReadConcatentatedBoxes(box_bytes, &media_log));
    ASSERT_TRUE(reader->ScanChildren());

    InBandAV2CodecConfigurationRecord box_config;
    EXPECT_EQ(reader->ReadChild(&box_config), direct_ok);
    if (direct_ok) {
      EXPECT_EQ(box_config.profile, direct_config.profile);
      EXPECT_EQ(box_config.level, direct_config.level);
    }
  }
}

FUZZ_TEST(Mp4Av2BoxFuzzTest, ParseInBandConfig)
    .WithDomains(fuzztest::Arbitrary<std::vector<uint8_t>>())
    .WithSeeds({
        {std::vector<uint8_t>(std::begin(kValidInBandAv2cPayload),
                              std::end(kValidInBandAv2cPayload))},
    });

void ParseVideoSampleEntry(const std::string& fourcc,
                           uint16_t width,
                           uint16_t height,
                           const std::vector<uint8_t>& child_boxes) {
  if (child_boxes.size() >
      static_cast<size_t>(std::numeric_limits<int32_t>::max()) - 86u) {
    return;
  }

  const std::vector<uint8_t> sample_entry_bytes =
      BuildVisualSampleEntryBox(fourcc, width, height, child_boxes);
  NullMediaLog media_log;
  std::unique_ptr<BoxReader> reader(
      BoxReader::ReadConcatentatedBoxes(sample_entry_bytes, &media_log));

  std::vector<VideoSampleEntry> entries;
  if (reader->ReadAllChildren(&entries)) {
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_TRUE(entries[0].IsFormatValid());
    EXPECT_EQ(entries[0].video_info.codec, VideoCodec::kAV2);
    EXPECT_GE(entries[0].video_info.profile, AV2PROFILE_MIN);
    EXPECT_LE(entries[0].video_info.profile, AV2PROFILE_MAX);
    EXPECT_LE(entries[0].video_info.level, 31u);
    if (fourcc == "av2E") {
      EXPECT_FALSE(entries[0].extra_data.empty());
    } else if (fourcc == "av2e") {
      EXPECT_TRUE(entries[0].extra_data.empty());
    }
  }
}

FUZZ_TEST(Mp4Av2BoxFuzzTest, ParseVideoSampleEntry)
    .WithDomains(fuzztest::ElementOf<std::string>({"av2E", "av2e", "av02"}),
                 fuzztest::Arbitrary<uint16_t>(),
                 fuzztest::Arbitrary<uint16_t>(),
                 fuzztest::Arbitrary<std::vector<uint8_t>>())
    .WithSeeds({
        {"av2E", uint16_t{1920}, uint16_t{1080},
         WrapMp4Box("av2C", kValidOutOfBandAv2CPayload)},
        {"av2e", uint16_t{1280}, uint16_t{720},
         WrapMp4Box("av2c", kValidInBandAv2cPayload)},
        {"av02", uint16_t{1920}, uint16_t{1080},
         WrapMp4Box("av2C", kValidOutOfBandAv2CPayload)},
        {"av02", uint16_t{1280}, uint16_t{720},
         WrapMp4Box("av2c", kValidInBandAv2cPayload)},
    });

}  // namespace
}  // namespace media::mp4
