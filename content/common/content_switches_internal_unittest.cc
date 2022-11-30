// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/content_switches_internal.h"

#include "base/command_line.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;

namespace content {

TEST(ContentSwitchesTest, FeaturesFromSwitch) {
  base::CommandLine cl(base::CommandLine::NO_PROGRAM);
  cl.AppendSwitchASCII("sw", "aaa");
  cl.AppendSwitchASCII("sw", "bb,cc,dd");
  cl.AppendSwitchASCII("sw", "eee");
  EXPECT_EQ(0u, FeaturesFromSwitch(cl, "nope").size());
  auto features = FeaturesFromSwitch(cl, "sw");
  EXPECT_THAT(features, ElementsAre("aaa", "bb", "cc", "dd", "eee"));
}

}  // namespace content
