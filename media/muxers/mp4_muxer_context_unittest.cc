// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/muxers/mp4_muxer_context.h"

#include <string>
#include <string_view>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "media/formats/mp4/writable_box_definitions.h"
#include "media/muxers/output_position_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(Mp4MuxerContextTest, Default) {
  base::test::SingleThreadTaskEnvironment task_environment;
  std::vector<uint8_t> written_data;
  base::RunLoop run_loop;

  auto output_position_tracker =
      std::make_unique<OutputPositionTracker>(base::BindLambdaForTesting(
          [&written_data, &run_loop](base::span<const uint8_t> data) {
            base::ranges::copy(data, std::back_inserter(written_data));
            run_loop.Quit();
          }));

  Mp4MuxerContext mp4_context(std::move(output_position_tracker));
  EXPECT_FALSE(mp4_context.GetVideoTrack().has_value());
  EXPECT_FALSE(mp4_context.GetAudioTrack().has_value());

  mp4_context.SetVideoTrack({1, 1111});
  mp4_context.SetAudioTrack({2, 2222});

  std::string str1 = "abc";
  mp4_context.GetOutputPositionTracker().WriteSpan(base::as_byte_span(str1));
  run_loop.Run();

  EXPECT_TRUE(mp4_context.GetVideoTrack().has_value());
  EXPECT_TRUE(mp4_context.GetAudioTrack().has_value());
  EXPECT_EQ(mp4_context.GetVideoTrack().value().index, 1u);
  EXPECT_EQ(mp4_context.GetAudioTrack().value().index, 2u);

  EXPECT_TRUE(mp4_context.GetVideoTrack().has_value());
  EXPECT_TRUE(mp4_context.GetAudioTrack().has_value());
  EXPECT_EQ(mp4_context.GetVideoTrack().value().timescale, 1111u);
  EXPECT_EQ(mp4_context.GetAudioTrack().value().timescale, 2222u);
  EXPECT_EQ(base::as_byte_span(str1), base::as_byte_span(written_data));
}

}  // namespace media
