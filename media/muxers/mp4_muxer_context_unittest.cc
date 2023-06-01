// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "media/formats/mp4/writable_box_definitions.h"
#include "media/muxers/mp4_muxer_context.h"
#include "media/muxers/output_position_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(Mp4MuxerContextTest, Default) {
  base::test::SingleThreadTaskEnvironment task_environment;
  std::string written_data;
  base::RunLoop run_loop;

  auto output_position_tracker =
      std::make_unique<OutputPositionTracker>(base::BindRepeating(
          [](std::string* written_data, base::OnceClosure run_loop_quit,
             base::StringPiece data) {
            written_data->append(data);
            std::move(run_loop_quit).Run();
          },
          &written_data, run_loop.QuitClosure()));

  Mp4MuxerContext mp4_context(std::move(output_position_tracker));
  EXPECT_FALSE(mp4_context.GetVideoIndex().has_value());
  EXPECT_FALSE(mp4_context.GetAudioIndex().has_value());

  mp4_context.SetVideoIndex(1);
  mp4_context.SetAudioIndex(2);

  std::string str1 = "abc";
  mp4_context.GetOutputPositionTracker().WriteString(str1);
  run_loop.Run();

  EXPECT_TRUE(mp4_context.GetVideoIndex().has_value());
  EXPECT_TRUE(mp4_context.GetAudioIndex().has_value());
  EXPECT_EQ(mp4_context.GetVideoIndex(), static_cast<size_t>(1));
  EXPECT_EQ(mp4_context.GetAudioIndex(), static_cast<size_t>(2));
  EXPECT_EQ(str1, written_data);
}

}  // namespace media
