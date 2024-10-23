// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_JS_FEATURES_WINDOW_ERROR_SCRIPT_ERROR_STACK_UTIL_H_
#define IOS_WEB_JS_FEATURES_WINDOW_ERROR_SCRIPT_ERROR_STACK_UTIL_H_

#import <string>

namespace script_error_stack_util {

// Constants exposed for testing
extern const char kCatchAndReportErrorsFrame[];
extern const char kLongStackTruncationSeparator[];
extern const char kStackFramesSeparator[];

// A struct holding the components of a script error stack frame.
struct FrameComponents {
  std::string function_name = "";
  std::string file_name = "";
  int line = 0;
  int column = 0;
};

// Returns a stack with only frames which are useful for debugging.
// Specifically, frames with no `function_name` or frames where the
// `function_name` is `kCatchAndReportErrorsFrame` are filtered out.
std::string FilterForUsefulStackFrames(std::string_view stack);

// Returns components of the top frame of the `stack`.
FrameComponents TopFrameComponentsFromStack(std::string_view stack);

// Truncates the middle of `stack` to ensure it is no larger than `max_size`.
std::string TruncateMiddle(std::string_view stack, unsigned long max_size);

}  // namespace script_error_stack_util

#endif  // IOS_WEB_JS_FEATURES_WINDOW_ERROR_SCRIPT_ERROR_STACK_UTIL_H_
