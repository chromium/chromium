// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "media/muxers/output_position_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(OutputPositionTrackerTest, OutputPositionTracker) {
  base::test::SingleThreadTaskEnvironment task_environment;
  std::string written_data;
  base::RunLoop run_loop;

  OutputPositionTracker buffer(base::BindRepeating(
      [](std::string* written_data, base::OnceClosure run_loop_quit,
         base::StringPiece data) {
        written_data->append(data);
        static int called_count = 0;
        if (++called_count == 3) {
          std::move(run_loop_quit).Run();
        }
      },
      &written_data, run_loop.QuitClosure()));

  std::string str1 = "abc";
  buffer.WriteString(str1);
  EXPECT_EQ(buffer.GetCurrentPos(), 3u);

  str1.resize(5);
  buffer.WriteString(str1);
  EXPECT_EQ(buffer.GetCurrentPos(), 8u);

  buffer.WriteString(str1);
  EXPECT_EQ(buffer.GetCurrentPos(), 13u);
  
  run_loop.Run();

  constexpr char kExpectedResult[] = {'a',  'b', 'c', 'a', 'b',  'c', '\0',
                                      '\0', 'a', 'b', 'c', '\0', '\0'};
  ASSERT_EQ(std::size(kExpectedResult), written_data.size());
  EXPECT_EQ(memcmp(kExpectedResult, written_data.data(), written_data.size()),
            0);
}

}  // namespace media
