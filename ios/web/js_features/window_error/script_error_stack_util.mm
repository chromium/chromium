// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_features/window_error/script_error_stack_util.h"

#import <string>
#import <vector>

#import "base/strings/string_number_conversions.h"
#import "base/strings/string_split.h"
#import "base/strings/string_util.h"
#import "base/strings/stringprintf.h"

namespace script_error_stack_util {

const char kCatchAndReportErrorsFrame[] = "catchAndReportErrors@";
const char kLongStackTruncationSeparator[] = "\n...\n";
const char kStackFramesSeparator[] = "\n";

namespace {

// Returns the stack frame `frame` split into its components.
FrameComponents SplitFrame(std::string_view frame) {
  FrameComponents result;

  std::vector<std::string_view> func_file_components = base::SplitStringPiece(
      frame, "@", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (func_file_components.size() == 2) {
    result.function_name = func_file_components[0];

    std::vector<std::string_view> file_components =
        base::SplitStringPiece(func_file_components[1], ":",
                               base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    if (file_components.size() == 3) {
      result.file_name = file_components[0];
      base::StringToInt(file_components[1], &result.line);
      base::StringToInt(file_components[2], &result.column);
    } else if (file_components.size() == 4) {
      result.file_name =
          base::StringPrintf("%s:%s", file_components[0], file_components[1]);
      base::StringToInt(file_components[2], &result.line);
      base::StringToInt(file_components[3], &result.column);
    }
  }

  return result;
}

}  // namespace

std::string FilterForUsefulStackFrames(std::string_view stack) {
  std::vector<std::string_view> useful_stack_frames;
  for (const auto& stack_frame : base::SplitStringPiece(
           stack, kStackFramesSeparator, base::TRIM_WHITESPACE,
           base::SPLIT_WANT_NONEMPTY)) {
    if (!base::StartsWith(stack_frame, "@") &&
        !base::StartsWith(stack_frame, kCatchAndReportErrorsFrame)) {
      useful_stack_frames.push_back(stack_frame);
    }
  }
  return base::JoinString(useful_stack_frames, kStackFramesSeparator);
}

FrameComponents TopFrameComponentsFromStack(std::string_view stack) {
  if (stack.length() == 0) {
    return FrameComponents();
  }

  size_t top_frame_end = stack.find(kStackFramesSeparator);
  if (top_frame_end != std::string::npos) {
    return SplitFrame(stack.substr(/*pos=*/0, top_frame_end));
  }

  return SplitFrame(stack);
}

std::string TruncateMiddle(std::string_view stack, unsigned long max_size) {
  // Truncate middle of stack to ensure the start and end are available.
  const unsigned long substring_size =
      std::floor((max_size - strlen(kLongStackTruncationSeparator)) / 2);
  const std::string_view begin = stack.substr(0, substring_size);
  const std::string_view end =
      stack.substr(stack.length() - substring_size, substring_size);

  return base::StringPrintf("%s%s%s", begin, kLongStackTruncationSeparator,
                            end);
}

}  // namespace script_error_stack_util
