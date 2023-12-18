// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/selection_state.h"

#include <sstream>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

TEST(SelectionStateTest, StreamOutput) {
  test::TaskEnvironment task_environment;
  // Just explicitly sanity check a couple of values.
  {
    std::stringstream string_stream;
    string_stream << SelectionState::kNone;
    EXPECT_EQ("None", string_stream.str());
  }
  {
    std::stringstream string_stream;
    string_stream << SelectionState::kStartAndEnd;
    EXPECT_EQ("StartAndEnd", string_stream.str());
  }
}

}  // namespace blink
