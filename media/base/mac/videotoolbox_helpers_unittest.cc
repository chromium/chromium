// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/mac/videotoolbox_helpers.h"

#include <CoreMedia/CoreMedia.h>

#include <algorithm>
#include <array>
#include <iterator>
#include <vector>

#include "base/apple/scoped_cftyperef.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media::video_toolbox {

namespace {

// Configuration from buck180p30.mp4.
constexpr auto kH264Sps = std::to_array<uint8_t>({
    0x67, 0x64, 0x00, 0x28, 0xac, 0xd1, 0x00, 0x78, 0x02,
    0x27, 0xe5, 0xc0, 0x44, 0x00, 0x00, 0x03, 0x00, 0x04,
    0x00, 0x00, 0x03, 0x00, 0xf0, 0x3c, 0x60, 0xc4, 0x48,
});
constexpr auto kH264Pps = std::to_array<uint8_t>({0x68, 0xeb, 0xef, 0x2c});
constexpr auto kLengthPrefixedH264Idr =
    std::to_array<uint8_t>({0x00, 0x00, 0x00, 0x03, 0x65, 0x88, 0x84});
// user_data_unregistered with a 16-byte UUID and no user data.
constexpr auto kAnnexBSeiNalu = std::to_array<uint8_t>({
    0x00, 0x00, 0x00, 0x01, 0x06, 0x05, 0x10, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x80,
});
constexpr auto kAnnexBStartCode =
    std::to_array<uint8_t>({0x00, 0x00, 0x00, 0x01});

std::vector<uint8_t> MakeExpectedAccessUnit(
    base::span<const uint8_t> sei_nalu) {
  std::vector<uint8_t> access_unit;
  auto append_nalu = [&access_unit](base::span<const uint8_t> nalu) {
    access_unit.insert(access_unit.end(), kAnnexBStartCode.begin(),
                       kAnnexBStartCode.end());
    access_unit.insert(access_unit.end(), nalu.begin(), nalu.end());
  };
  append_nalu(kH264Sps);
  append_nalu(kH264Pps);
  access_unit.insert(access_unit.end(), sei_nalu.begin(), sei_nalu.end());
  append_nalu(base::span(kLengthPrefixedH264Idr).subspan<4>());
  return access_unit;
}

base::apple::ScopedCFTypeRef<CMSampleBufferRef> MakeH264SampleBuffer() {
  const uint8_t* parameter_sets[] = {kH264Sps.data(), kH264Pps.data()};
  const size_t parameter_set_sizes[] = {kH264Sps.size(), kH264Pps.size()};
  base::apple::ScopedCFTypeRef<CMFormatDescriptionRef> format_description;
  CHECK_EQ(CMVideoFormatDescriptionCreateFromH264ParameterSets(
               kCFAllocatorDefault, std::size(parameter_sets), parameter_sets,
               parameter_set_sizes, /*NALUnitHeaderLength=*/4,
               format_description.InitializeInto()),
           noErr);

  base::apple::ScopedCFTypeRef<CMBlockBufferRef> data_buffer;
  CHECK_EQ(CMBlockBufferCreateWithMemoryBlock(
               kCFAllocatorDefault, /*memoryBlock=*/nullptr,
               kLengthPrefixedH264Idr.size(), kCFAllocatorDefault,
               /*customBlockSource=*/nullptr,
               /*offsetToData=*/0, kLengthPrefixedH264Idr.size(),
               /*flags=*/0, data_buffer.InitializeInto()),
           noErr);
  CHECK_EQ(CMBlockBufferReplaceDataBytes(
               kLengthPrefixedH264Idr.data(), data_buffer.get(),
               /*offsetIntoDestination=*/0, kLengthPrefixedH264Idr.size()),
           noErr);

  const CMSampleTimingInfo timing_info = {
      .duration = kCMTimeInvalid,
      .presentationTimeStamp = kCMTimeZero,
      .decodeTimeStamp = kCMTimeInvalid,
  };
  const size_t sample_size = kLengthPrefixedH264Idr.size();
  base::apple::ScopedCFTypeRef<CMSampleBufferRef> sample_buffer;
  CHECK_EQ(CMSampleBufferCreateReady(
               kCFAllocatorDefault, data_buffer.get(), format_description.get(),
               /*numSamples=*/1, /*numSampleTimingEntries=*/1, &timing_info,
               /*numSampleSizeEntries=*/1, &sample_size,
               sample_buffer.InitializeInto()),
           noErr);
  return sample_buffer;
}

}  // namespace

TEST(VideoToolboxHelpersTest, InsertsSeiNaluAfterParameterSets) {
  auto sample_buffer = MakeH264SampleBuffer();
  std::array<char, 128> output;
  size_t used_buffer_size = 0;
  const std::vector<uint8_t> expected_access_unit =
      MakeExpectedAccessUnit(kAnnexBSeiNalu);

  ASSERT_TRUE(CopySampleBufferToAnnexBBuffer(
      VideoCodec::kH264, sample_buffer.get(), /*keyframe=*/true, kAnnexBSeiNalu,
      output.size(), output.data(), &used_buffer_size));

  EXPECT_EQ(used_buffer_size, expected_access_unit.size());
  EXPECT_TRUE(std::ranges::equal(
      base::as_bytes(base::span(output).first(used_buffer_size)),
      expected_access_unit));
}

}  // namespace media::video_toolbox
