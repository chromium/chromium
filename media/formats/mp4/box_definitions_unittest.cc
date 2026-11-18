// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mp4/box_definitions.h"

#include <stdint.h>

#include <array>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/check_op.h"
#include "base/containers/extend.h"
#include "base/containers/span.h"
#include "media/base/channel_layout.h"
#include "media/base/mock_media_log.h"
#include "media/formats/mp4/box_reader.h"
#include "media/formats/mp4/parse_result.h"
#include "media/media_buildflags.h"
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

#if BUILDFLAG(ENABLE_PLATFORM_IAMF_AUDIO) || BUILDFLAG(ENABLE_IAMF_TOOLS)
namespace {

constexpr uint8_t kObuAudioElement = 1;
constexpr uint8_t kObuSequenceHeader = 31;
constexpr uint8_t kSceneBased = 1;
constexpr std::array<uint8_t, 1> kNoParams = {0};

std::vector<uint8_t> Concat(
    std::initializer_list<base::span<const uint8_t>> parts) {
  std::vector<uint8_t> out;
  for (base::span<const uint8_t> part : parts) {
    base::Extend(out, part);
  }
  return out;
}

std::vector<uint8_t> Obu(uint8_t type, base::span<const uint8_t> payload) {
  return Concat({std::vector<uint8_t>{static_cast<uint8_t>(type << 3),
                                      static_cast<uint8_t>(payload.size())},
                 payload});
}

std::vector<uint8_t> Layer(IamfLoudspeakerLayout loudspeaker_layout,
                           bool has_output_gain = false) {
  static constexpr uint8_t kSubstreamCount = 1;
  static constexpr uint8_t kCoupledSubstreamCount = 0;
  static constexpr std::array<uint8_t, 3> kOutputGainFlagsAndGain = {0, 0, 0};
  static constexpr uint8_t kExpandedLoudspeakerLayout = 0;

  std::vector<uint8_t> layer = {
      static_cast<uint8_t>(static_cast<uint8_t>(loudspeaker_layout) << 4 |
                           (has_output_gain ? 0x08 : 0x00)),
      kSubstreamCount, kCoupledSubstreamCount};
  if (has_output_gain) {
    base::Extend(layer, kOutputGainFlagsAndGain);
  }
  if (loudspeaker_layout == IamfLoudspeakerLayout::kExpanded) {
    layer.push_back(kExpandedLoudspeakerLayout);
  }
  return layer;
}

std::vector<uint8_t> ChannelElement(
    std::initializer_list<base::span<const uint8_t>> layers,
    base::span<const uint8_t> params = kNoParams) {
  static constexpr uint8_t kAudioElementId = 0;
  static constexpr uint8_t kAudioElementType = 0;
  static constexpr uint8_t kCodecConfigId = 0;
  static constexpr uint8_t kNumSubstreams = 1;
  static constexpr uint8_t kAudioSubstreamId = 0;
  return Obu(
      kObuAudioElement,
      Concat({std::vector<uint8_t>{kAudioElementId, kAudioElementType,
                                   kCodecConfigId, kNumSubstreams,
                                   kAudioSubstreamId},
              params,
              std::vector<uint8_t>{static_cast<uint8_t>(layers.size() << 5)},
              Concat(layers)}));
}

std::vector<uint8_t> IacbBox(base::span<const uint8_t> obus) {
  static constexpr uint8_t kConfigurationVersion = 1;
  const std::vector<uint8_t> descriptors =
      Concat({Obu(kObuSequenceHeader, {'i', 'a', 'm', 'f', 0, 0}), obus});
  const size_t box_size = 8 + 2 + descriptors.size();
  CHECK_LT(descriptors.size(), 128u);
  return Concat({std::vector<uint8_t>{0, 0, 0, static_cast<uint8_t>(box_size),
                                      'i', 'a', 'c', 'b', kConfigurationVersion,
                                      static_cast<uint8_t>(descriptors.size())},
                 descriptors});
}

struct IamfChannelLayoutCase {
  std::string name;
  std::vector<uint8_t> obus;
  std::optional<ChannelLayoutConfig> expected;
};

std::vector<IamfChannelLayoutCase> IamfChannelLayoutCases() {
  using Layout = IamfLoudspeakerLayout;
  const ChannelLayoutConfig k5_1 =
      ChannelLayoutConfig::FromLayout<CHANNEL_LAYOUT_5_1>();
  const ChannelLayoutConfig k7_1_4 =
      ChannelLayoutConfig::FromLayout<CHANNEL_LAYOUT_7_1_4>();

  const std::vector<uint8_t> kDemixingParam = {1, 0, 1, 0x00, 8, 0,
                                               2, 4, 4, 0,    0};
  const std::vector<uint8_t> kConstantSubblockParam = {1, 0, 1, 0x00,
                                                       8, 4, 0, 0};
  const std::vector<uint8_t> kReconGainParam = {2, 1, 1, 0x80};
  const std::vector<uint8_t> kExtensionParam = {9, 2, 0xaa, 0xbb};
  const std::vector<uint8_t> kParams =
      Concat({std::vector<uint8_t>{3}, kDemixingParam, kReconGainParam,
              kExtensionParam});
  const std::vector<uint8_t> kConstantSubblockParams =
      Concat({std::vector<uint8_t>{1}, kConstantSubblockParam});

  return {
      {"Mono", ChannelElement({Layer(Layout::kMono)}),
       ChannelLayoutConfig::Mono()},
      {"Binaural", ChannelElement({Layer(Layout::kBinaural)}),
       ChannelLayoutConfig::Stereo()},
      {"Layout5_1", ChannelElement({Layer(Layout::k5_1)}), k5_1},
      {"Layout5_1_4", ChannelElement({Layer(Layout::k5_1_4)}),
       ChannelLayoutConfig::FromLayout<CHANNEL_LAYOUT_5_1_4>()},
      {"Layout7_1", ChannelElement({Layer(Layout::k7_1)}),
       ChannelLayoutConfig::FromLayout<CHANNEL_LAYOUT_7_1>()},
      {"Layout7_1_4", ChannelElement({Layer(Layout::k7_1_4)}), k7_1_4},
      {"TopLayerOfScalableElement",
       ChannelElement({Layer(Layout::kStereo), Layer(Layout::k5_1),
                       Layer(Layout::k7_1_4)}),
       k7_1_4},
      {"LayerWithOutputGain",
       ChannelElement({Layer(Layout::kStereo, true), Layer(Layout::k5_1)}),
       k5_1},
      {"LayerAfterExpandedLayout",
       ChannelElement({Layer(Layout::kExpanded), Layer(Layout::k5_1)}), k5_1},
      {"ParameterDefinitions", ChannelElement({Layer(Layout::k5_1)}, kParams),
       k5_1},
      {"ConstantSubblockDuration",
       ChannelElement({Layer(Layout::k5_1)}, kConstantSubblockParams), k5_1},
      {"MixGainParamDefinition", ChannelElement({Layer(Layout::k5_1)}, {1, 0}),
       std::nullopt},
      {"LargestElementFirst",
       Concat({ChannelElement({Layer(Layout::k7_1_4)}),
               ChannelElement({Layer(Layout::k5_1)})}),
       k7_1_4},
      {"LargestElementLast",
       Concat({ChannelElement({Layer(Layout::k5_1)}),
               ChannelElement({Layer(Layout::k7_1_4)})}),
       k7_1_4},
      {"NoAudioElement", {}, std::nullopt},
      {"SceneBasedElement",
       Obu(kObuAudioElement, Concat({std::vector<uint8_t>{0, kSceneBased << 5,
                                                          0, 1, 0, 0, 1 << 5},
                                     Layer(Layout::k5_1)})),
       std::nullopt},
      {"LayoutWithoutChannelLayout", ChannelElement({Layer(Layout::k5_1_2)}),
       std::nullopt},
      {"ExpandedLayout", ChannelElement({Layer(Layout::kExpanded)}),
       std::nullopt},
      {"TruncatedElement", Obu(kObuAudioElement, {0, 0, 0}), std::nullopt},
  };
}

}  // namespace

class IamfSpecificBoxChannelLayoutTest
    : public BoxDefinitionsTest,
      public testing::WithParamInterface<IamfChannelLayoutCase> {};

TEST_P(IamfSpecificBoxChannelLayoutTest, ChannelLayout) {
  const std::vector<uint8_t> box = IacbBox(GetParam().obus);

  std::unique_ptr<BoxReader> reader(
      BoxReader::ReadConcatentatedBoxes(box, &media_log_));
  ASSERT_TRUE(reader->ScanChildren());
  IamfSpecificBox iacb;
  ASSERT_TRUE(reader->ReadChild(&iacb));
  EXPECT_EQ(iacb.channel_layout, GetParam().expected);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    IamfSpecificBoxChannelLayoutTest,
    testing::ValuesIn(IamfChannelLayoutCases()),
    [](const testing::TestParamInfo<IamfChannelLayoutCase>& info) {
      return info.param.name;
    });

TEST_F(BoxDefinitionsTest, IamfSpecificBoxObuExtensionExceedsObuSize) {
  static constexpr uint8_t kExtensionFlag = 1;
  static constexpr uint8_t kObuSize = 0;
  static constexpr uint8_t kExtensionHeaderSize = 0;
  const std::vector<uint8_t> box =
      IacbBox({{static_cast<uint8_t>(kObuAudioElement << 3 | kExtensionFlag),
                kObuSize, kExtensionHeaderSize}});

  std::unique_ptr<BoxReader> reader(
      BoxReader::ReadConcatentatedBoxes(box, &media_log_));
  ASSERT_TRUE(reader->ScanChildren());
  IamfSpecificBox iacb;
  EXPECT_FALSE(reader->ReadChild(&iacb));
}
#endif  // BUILDFLAG(ENABLE_PLATFORM_IAMF_AUDIO) || ...

}  // namespace media::mp4
