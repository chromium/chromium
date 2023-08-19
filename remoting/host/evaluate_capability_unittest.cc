// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/evaluate_capability.h"

#include "base/containers/contains.h"
#include "base/strings/string_util.h"
#include "remoting/host/base/switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

// New line character varies on different platform, so normalize the output
// here to reduce the complexity of comparing.
std::string NormalizeOutput(std::string output) {
  base::ReplaceSubstringsAfterOffset(&output, 0, "\r\n", "\n");
  base::ReplaceSubstringsAfterOffset(&output, 0, "\r", "\n");
  // Windows (evilly) use \r\n to replace \n, so we will end up with two \n.
  while (base::Contains(output, "\n\n")) {
    base::ReplaceSubstringsAfterOffset(&output, 0, "\n\n", "\n");
  }
  return output;
}

}  // namespace

// TODO(zijiehe): Find out the root cause of the unexpected failure of this test
// case. See http://crbug.com/750330.
TEST(EvaluateCapabilityTest, DISABLED_ShouldReturnCrashResult) {
  ASSERT_NE(EvaluateCapability("crash"), 0);
}

TEST(EvaluateCapabilityTest, ShouldReturnExitCodeAndOutput) {
  std::string output;
  ASSERT_EQ(EvaluateCapability("test", &output), 234);
  ASSERT_EQ(
      "In EvaluateTest(): Line 1\n"
      "In EvaluateTest(): Line 2",
      NormalizeOutput(output));
}

TEST(EvaluateCapabilityTest, ShouldReturnSuccessAndOutput) {
  std::string output;
  ASSERT_EQ(EvaluateCapability("success", &output), 0);
  ASSERT_EQ("Success\n", NormalizeOutput(output));
}

}  // namespace remoting
