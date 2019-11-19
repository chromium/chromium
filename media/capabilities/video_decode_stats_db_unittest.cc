// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "media/base/video_codecs.h"
#include "media/capabilities/bucket_utility.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

#include "media/capabilities/video_decode_stats_db.h"

namespace media {

VideoDecodeStatsDB::VideoDescKey MakeKey(VideoCodecProfile codec_profile,
                                         const gfx::Size& size,
                                         int frame_rate,
                                         std::string key_system,
                                         bool use_hw_secure_codecs) {
  return VideoDecodeStatsDB::VideoDescKey::MakeBucketedKey(
      codec_profile, size, frame_rate, key_system, use_hw_secure_codecs);
}

TEST(VideoDecodeStatsDBTest, KeySerialization) {
  // Serialized key with empty KeySystem string should not mention encryption
  // fields.
  auto keyA =
      MakeKey(H264PROFILE_BASELINE, gfx::Size(1280, 720), 30, "", false);
  ASSERT_EQ("0|1280x720|30", keyA.Serialize());

  // Same as above + KeySystem. Serialize should now show KeySystem name +
  // mention of hw_secure.
  auto keyB = MakeKey(H264PROFILE_BASELINE, gfx::Size(1280, 720), 30,
                      "org.w3.clearkey", false);
  ASSERT_EQ("0|1280x720|30|org.w3.clearkey|not_hw_secure", keyB.Serialize());

  // Different KeySystem, different hw_secure status.
  auto keyC = MakeKey(H264PROFILE_BASELINE, gfx::Size(1280, 720), 30,
                      "com.widevine.alpha", true);
  ASSERT_EQ("0|1280x720|30|com.widevine.alpha|is_hw_secure", keyC.Serialize());

  // Different everything for good measure.
  auto keyD = MakeKey(VP9PROFILE_PROFILE0, gfx::Size(640, 360), 25,
                      "com.example", false);
  ASSERT_EQ("12|640x360|25|com.example|not_hw_secure", keyD.Serialize());
}

TEST(VideoDecodeStatsDBTest, OperatorEquals) {
  auto keyA =
      MakeKey(H264PROFILE_BASELINE, gfx::Size(1280, 720), 30, "", false);
  ASSERT_EQ(keyA, keyA);

  auto keyB = keyA;
  ASSERT_EQ(keyA, keyB);

  // Vary each of the fields in keyA and verify != keyA
  ASSERT_NE(keyA,
            MakeKey(VP9PROFILE_PROFILE0, gfx::Size(1280, 720), 30, "", false));
  ASSERT_NE(keyA,
            MakeKey(H264PROFILE_BASELINE, gfx::Size(640, 360), 30, "", false));
  ASSERT_NE(keyA,
            MakeKey(H264PROFILE_BASELINE, gfx::Size(1280, 720), 25, "", false));
  ASSERT_NE(keyA, MakeKey(H264PROFILE_BASELINE, gfx::Size(1280, 720), 30,
                          "com.example", false));
  // Hits DCHECK - hw_secure_codecs cannot be true when key_system = "".
  // ASSERT_NE(keyA, MakeKey(H264PROFILE_BASELINE, gfx::Size(1280, 720), 30, "",
  // true));

  // Verify key's are equal when their bucketed values match.
  ASSERT_EQ(keyA.frame_rate, GetFpsBucket(29));
  ASSERT_EQ(keyA.size, GetSizeBucket(gfx::Size(1279, 719)));
  ASSERT_EQ(keyA,
            MakeKey(H264PROFILE_BASELINE, gfx::Size(1279, 719), 29, "", false));
}

TEST(VideoDecodeStatsDBTest, KeyBucketting) {
  // Create a key with with unusual size and frame rate.
  auto keyA =
      MakeKey(H264PROFILE_BASELINE, gfx::Size(1279, 719), 29, "", false);
  // Verify the created key chooses nearby buckets instead of raw values.
  ASSERT_EQ(30, keyA.frame_rate);
  ASSERT_EQ(gfx::Size(1280, 720), keyA.size);

  // See VideoDecodeStatsReporterTest for more elaborate testing of the
  // bucketing logic.
}

}  // namespace media
