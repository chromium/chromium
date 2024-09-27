// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/muxers/output_position_tracker.h"

#include <string>
#include <string_view>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(OutputPositionTrackerTest, OutputPositionTracker) {
  base::test::SingleThreadTaskEnvironment task_environment;
  std::vector<uint8_t> written_data;
  base::RunLoop run_loop;

  OutputPositionTracker buffer(base::BindLambdaForTesting(
      [&written_data, &run_loop](base::span<const uint8_t> data) {
        written_data.insert(written_data.end(), data.begin(), data.end());
        static int called_count = 0;
        if (++called_count == 3) {
          run_loop.Quit();
        }
      }));

  base::span<const uint8_t> span = base::byte_span_from_cstring("abc\0\0");
  buffer.WriteSpan(span.subspan(0, 3));
  EXPECT_EQ(buffer.GetCurrentPos(), 3u);

  buffer.WriteSpan(span);
  EXPECT_EQ(buffer.GetCurrentPos(), 8u);

  buffer.WriteSpan(span);
  EXPECT_EQ(buffer.GetCurrentPos(), 13u);

  run_loop.Run();

  constexpr char kExpectedResult[] = {'a',  'b', 'c', 'a', 'b',  'c', '\0',
                                      '\0', 'a', 'b', 'c', '\0', '\0'};
  ASSERT_EQ(std::size(kExpectedResult), written_data.size());
  EXPECT_EQ(memcmp(kExpectedResult, written_data.data(), written_data.size()),
            0);
}

}  // namespace media
