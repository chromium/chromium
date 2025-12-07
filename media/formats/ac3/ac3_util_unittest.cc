// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/ac3/ac3_util.h"

#include <algorithm>

#include "base/files/file_util.h"
#include "media/base/test_data_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

static constexpr size_t kPcmFrameCount = 1536;

struct Ac3StreamInfo {
  size_t size;
  size_t pcm_frame_count;
};

}  // namespace anonymous

TEST(Ac3UtilTest, ParseTotalAc3SampleCount) {
  char buffer[8192];
  constexpr Ac3StreamInfo ac3StreamInfo[] = {
      {834, 1 * kPcmFrameCount},
      {1670, 2 * kPcmFrameCount},
      {2506, 3 * kPcmFrameCount},
  };

  for (const Ac3StreamInfo& info : ac3StreamInfo) {
    std::ranges::fill(buffer, 0);
    int data_size =
        base::ReadFile(GetTestDataFilePath("bear.ac3"), buffer, info.size);

    ASSERT_GT(data_size, 0);

    EXPECT_EQ(
        info.pcm_frame_count,
        ::media::Ac3Util::ParseTotalAc3SampleCount(
            base::as_byte_span(buffer).first(static_cast<size_t>(data_size))));
  }
}

TEST(Ac3UtilTest, ParseTotalEac3SampleCount) {
  char buffer[8192];
  constexpr Ac3StreamInfo ac3StreamInfo[] = {
      {870, 1 * kPcmFrameCount},
      {1742, 2 * kPcmFrameCount},
      {2612, 3 * kPcmFrameCount},
  };

  for (const Ac3StreamInfo& info : ac3StreamInfo) {
    std::ranges::fill(buffer, 0);
    int data_size =
        base::ReadFile(GetTestDataFilePath("bear.eac3"), buffer, info.size);

    ASSERT_GT(data_size, 0);

    EXPECT_EQ(
        info.pcm_frame_count,
        ::media::Ac3Util::ParseTotalEac3SampleCount(
            base::as_byte_span(buffer).first(static_cast<size_t>(data_size))));
  }
}

TEST(Ac3UtilTest, ParseTotalEac3SampleCount_MissingData) {
  char buffer[8192];
  constexpr Ac3StreamInfo ac3StreamInfo[] = {
      {870, 1 * kPcmFrameCount},
      {1742, 2 * kPcmFrameCount},
      {2612, 3 * kPcmFrameCount},
  };

  for (const Ac3StreamInfo& info : ac3StreamInfo) {
    std::ranges::fill(buffer, 0);
    int data_size =
        base::ReadFile(GetTestDataFilePath("bear.eac3"), buffer, info.size);

    ASSERT_GT(data_size, 0);

    auto valid_file =
        base::as_byte_span(buffer).first(static_cast<size_t>(data_size));

    // Remove 1 pcm data frame from the end of the file.
    auto missing_frame_file = valid_file.last(valid_file.size() - 1u);

    // The last `kPcmFrameCount` should not be included in the total frame count
    // if `Ac3Util` detects insufficient data.
    auto expected_partial_frame_count = info.pcm_frame_count - kPcmFrameCount;

    EXPECT_EQ(expected_partial_frame_count,
              ::media::Ac3Util::ParseTotalEac3SampleCount(missing_frame_file));

    // We should also skip the first `kPcmFrameCount` if the first header is
    // invalid.
    auto bad_first_header_file = valid_file.subspan<1u>();
    EXPECT_EQ(
        expected_partial_frame_count,
        ::media::Ac3Util::ParseTotalEac3SampleCount(bad_first_header_file));
  }
}

TEST(Ac3UtilTest, ParseTotalAc3SampleCount_MissingData) {
  char buffer[8192];
  constexpr Ac3StreamInfo ac3StreamInfo[] = {
      {834, 1 * kPcmFrameCount},
      {1670, 2 * kPcmFrameCount},
      {2506, 3 * kPcmFrameCount},
  };

  for (const Ac3StreamInfo& info : ac3StreamInfo) {
    std::ranges::fill(buffer, 0);
    int data_size =
        base::ReadFile(GetTestDataFilePath("bear.ac3"), buffer, info.size);

    ASSERT_GT(data_size, 0);

    auto valid_file =
        base::as_byte_span(buffer).first(static_cast<size_t>(data_size));

    // Remove 1 pcm data frame from the end of the file.
    auto missing_frame_file = valid_file.last(valid_file.size() - 1u);

    // The last `kPcmFrameCount` should not be included in the total frame count
    // if `Ac3Util` detects insufficient data.
    auto expected_partial_frame_count = info.pcm_frame_count - kPcmFrameCount;

    EXPECT_EQ(expected_partial_frame_count,
              ::media::Ac3Util::ParseTotalAc3SampleCount(missing_frame_file));

    // We should also skip the first `kPcmFrameCount` if the first header is
    // invalid.
    auto bad_first_header_file = valid_file.subspan<1u>();
    EXPECT_EQ(
        expected_partial_frame_count,
        ::media::Ac3Util::ParseTotalAc3SampleCount(bad_first_header_file));
  }
}

TEST(Ac3UtilTest, ParseTotalAc3SampleCount_NoData) {
  std::vector<uint8_t> buffer;
  EXPECT_EQ(0u, ::media::Ac3Util::ParseTotalAc3SampleCount(
                    base::as_byte_span(buffer)));
}

TEST(Ac3UtilTest, ParseTotalEac3SampleCount_NoData) {
  std::vector<uint8_t> buffer;
  EXPECT_EQ(0u, ::media::Ac3Util::ParseTotalEac3SampleCount(
                    base::as_byte_span(buffer)));
}

}  // namespace media
