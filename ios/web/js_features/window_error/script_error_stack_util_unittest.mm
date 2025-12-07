// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_features/window_error/script_error_stack_util.h"

#import "testing/platform_test.h"

using script_error_stack_util::FilterForUsefulStackFrames;
using script_error_stack_util::FrameComponents;
using script_error_stack_util::kCatchAndReportErrorsFrame;
using script_error_stack_util::TopFrameComponentsFromStack;
using script_error_stack_util::TruncateMiddle;

namespace web {

static const char kSampleStack[] = "findElementAtPoint@user-script:7:374:20\n\
    findElementAtPointInPageCoordinates@user-script:7:352:55\n\
    catchAndReportErrors@user-script:50:40:33\n\
    @user-script:50:75:54\n\
    findElementAtPoint@user-script:8:33:72\n\
    catchAndReportErrors@user-script:50:40:33\n\
    @user-script:50:75:54\n\
    global code@https://example.com/:1:40";

typedef PlatformTest ScriptErrorStackUtilTest;

// Tests that FilterForUsefulStackFrames filters out frames as expected.
TEST_F(ScriptErrorStackUtilTest, FilterForUsefulStackFrames) {
  std::string filtered_stack = FilterForUsefulStackFrames(kSampleStack);
  EXPECT_LT(filtered_stack.length(), strlen(kSampleStack));

  EXPECT_EQ(std::string::npos, filtered_stack.find(kCatchAndReportErrorsFrame));
}

// Tests that TopFrameComponentsFromStack returns the expected frame components
// when the filename contains `:`.
TEST_F(ScriptErrorStackUtilTest, FrameComponentsFromUserScriptFrame) {
  FrameComponents components = TopFrameComponentsFromStack(kSampleStack);
  EXPECT_EQ("findElementAtPoint", components.function_name);
  EXPECT_EQ("user-script:7", components.file_name);
  EXPECT_EQ(374, components.line);
  EXPECT_EQ(20, components.column);
}

// Tests that TopFrameComponentsFromStack returns the expected frame components
// when the filename is a URL.
TEST_F(ScriptErrorStackUtilTest, FrameComponentsFromURLFrame) {
  FrameComponents components =
      TopFrameComponentsFromStack("global code@https://example.com/:1:40");
  EXPECT_EQ("global code", components.function_name);
  EXPECT_EQ("https://example.com/", components.file_name);
  EXPECT_EQ(1, components.line);
  EXPECT_EQ(40, components.column);
}

// Tests that large stacks are correctly reduced in size.
TEST_F(ScriptErrorStackUtilTest, TruncateMiddle) {
  unsigned long truncated_size = strlen(kSampleStack) / 2;
  std::string truncated_stack = TruncateMiddle(kSampleStack, truncated_size);
  EXPECT_EQ(truncated_size, truncated_stack.length() + 1);
}

}  // namespace web
