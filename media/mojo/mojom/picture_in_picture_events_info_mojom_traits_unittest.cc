// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/picture_in_picture_events_info_mojom_traits.h"

#include "media/base/picture_in_picture_events_info.h"
#include "media/mojo/mojom/media_types.mojom.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(AutoPipInfoStructTraitsTest, ValidAutoPipInfo) {
  const media::PictureInPictureEventsInfo::AutoPipInfo
      auto_picture_in_picture_info{
          .auto_pip_reason =
              media::PictureInPictureEventsInfo::AutoPipReason::kMediaPlayback,
          .has_audio_focus = true,
          .is_playing = true,
          .was_recently_audible = true,
          .has_safe_url = true,
          .meets_media_engagement_conditions = true,
          .blocked_due_to_content_setting = true,
      };
  media::PictureInPictureEventsInfo::AutoPipInfo output;

  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<media::mojom::AutoPipInfo>(
      auto_picture_in_picture_info, output));

  EXPECT_EQ(output.auto_pip_reason,
            auto_picture_in_picture_info.auto_pip_reason);
  EXPECT_EQ(output.has_audio_focus,
            auto_picture_in_picture_info.has_audio_focus);
  EXPECT_EQ(output.is_playing, auto_picture_in_picture_info.is_playing);
  EXPECT_EQ(output.was_recently_audible,
            auto_picture_in_picture_info.was_recently_audible);
  EXPECT_EQ(output.has_safe_url, auto_picture_in_picture_info.has_safe_url);
  EXPECT_EQ(output.meets_media_engagement_conditions,
            auto_picture_in_picture_info.meets_media_engagement_conditions);
  EXPECT_EQ(output.blocked_due_to_content_setting,
            auto_picture_in_picture_info.blocked_due_to_content_setting);
}

TEST(AutoPipInfoStructTraitsTest, DefaultAutoPipInfo) {
  const media::PictureInPictureEventsInfo::AutoPipInfo
      auto_picture_in_picture_info{};
  media::PictureInPictureEventsInfo::AutoPipInfo output;

  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<media::mojom::AutoPipInfo>(
      auto_picture_in_picture_info, output));

  EXPECT_EQ(output.auto_pip_reason,
            media::PictureInPictureEventsInfo::AutoPipReason::kUnknown);
  EXPECT_FALSE(output.has_audio_focus);
  EXPECT_FALSE(output.is_playing);
  EXPECT_FALSE(output.was_recently_audible);
  EXPECT_FALSE(output.has_safe_url);
  EXPECT_FALSE(output.meets_media_engagement_conditions);
  EXPECT_FALSE(output.blocked_due_to_content_setting);
}
}  // namespace media
