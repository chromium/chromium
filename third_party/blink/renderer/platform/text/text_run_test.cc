// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/text/text_run.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(TextRunTest, IndexOfSubRun) {
  TextRun run("1234567890");
  EXPECT_EQ(0u, run.IndexOfSubRun(run.SubRun(0, 4)));
  EXPECT_EQ(4u, run.IndexOfSubRun(run.SubRun(4, 4)));
  EXPECT_EQ(6u, run.IndexOfSubRun(run.SubRun(6, 4)));
  const unsigned kNotSubRun = std::numeric_limits<unsigned>::max();
  EXPECT_EQ(kNotSubRun, run.IndexOfSubRun(run.SubRun(7, 4)));
  EXPECT_EQ(kNotSubRun, run.IndexOfSubRun(TextRun("1")));
  EXPECT_EQ(kNotSubRun, run.IndexOfSubRun(TextRun(u"1")));

  TextRun run16(u"1234567890");
  EXPECT_EQ(0u, run16.IndexOfSubRun(run16.SubRun(0, 4)));
  EXPECT_EQ(4u, run16.IndexOfSubRun(run16.SubRun(4, 4)));
  EXPECT_EQ(6u, run16.IndexOfSubRun(run16.SubRun(6, 4)));
  EXPECT_EQ(kNotSubRun, run16.IndexOfSubRun(run16.SubRun(7, 4)));
  EXPECT_EQ(kNotSubRun, run16.IndexOfSubRun(TextRun("1")));
  EXPECT_EQ(kNotSubRun, run16.IndexOfSubRun(TextRun(u"1")));
}

}  // namespace blink
