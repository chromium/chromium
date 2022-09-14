// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/ac3/ac3_util.h"

#include "base/files/file_util.h"
#include "media/base/test_data_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

struct Ac3StreamInfo {
  size_t size;
  int pcm_frame_count;
};

}  // namespace anonymous

TEST(Ac3UtilTest, ParseTotalAc3SampleCount) {
  char buffer[8192];
  const int buffer_size = sizeof(buffer);
  const Ac3StreamInfo ac3StreamInfo[] = {
      {834, 1 * 1536}, {1670, 2 * 1536}, {2506, 3 * 1536},
  };

  for (const Ac3StreamInfo& info : ac3StreamInfo) {
    memset(buffer, 0, buffer_size);
    int data_size =
        base::ReadFile(GetTestDataFilePath("bear.ac3"), buffer, info.size);

    EXPECT_EQ(info.pcm_frame_count,
              ::media::Ac3Util::ParseTotalAc3SampleCount(
                  reinterpret_cast<const uint8_t*>(buffer), data_size));
  }
}

TEST(Ac3UtilTest, ParseTotalEac3SampleCount) {
  char buffer[8192];
  const int buffer_size = sizeof(buffer);
  const Ac3StreamInfo ac3StreamInfo[] = {
      {870, 1 * 1536}, {1742, 2 * 1536}, {2612, 3 * 1536},
  };

  for (const Ac3StreamInfo& info : ac3StreamInfo) {
    memset(buffer, 0, buffer_size);
    int data_size =
        base::ReadFile(GetTestDataFilePath("bear.eac3"), buffer, info.size);

    EXPECT_EQ(info.pcm_frame_count,
              ::media::Ac3Util::ParseTotalEac3SampleCount(
                  reinterpret_cast<const uint8_t*>(buffer), data_size));
  }
}

}  // namespace media
