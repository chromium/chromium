// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stdint.h>

#include "media/base/video_frame.h"
#include "media/cast/test/utility/audio_utility.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace cast {
namespace test {
namespace {

TEST(AudioTimestampTest, Small) {
  std::vector<float> samples(480);
  for (int32_t in_timestamp = 0; in_timestamp < 65536; in_timestamp += 2077) {
    EncodeTimestamp(in_timestamp, 0, samples.size(), &samples.front());
    uint16_t out_timestamp;
    EXPECT_TRUE(
        DecodeTimestamp(&samples.front(), samples.size(), &out_timestamp));
    ASSERT_EQ(in_timestamp, out_timestamp);
  }
}

TEST(AudioTimestampTest, Negative) {
  std::vector<float> samples(480);
  uint16_t out_timestamp;
  EXPECT_FALSE(
      DecodeTimestamp(&samples.front(), samples.size(), &out_timestamp));
}

TEST(AudioTimestampTest, CheckPhase) {
  std::vector<float> samples(4800);
  EncodeTimestamp(4711, 0, samples.size(), &samples.front());
  for (size_t i = 0; i < samples.size() - 240; i += 143) {
    uint16_t out_timestamp;
    EXPECT_TRUE(DecodeTimestamp(&samples.front() + i, samples.size() - i,
                                &out_timestamp));
    ASSERT_EQ(4711, out_timestamp);
  }
}

}  // namespace
}  // namespace test
}  // namespace cast
}  // namespace media
