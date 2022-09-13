// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/stack_frame.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::UTF8ToUTF16;

namespace extensions {

namespace {

void AssertStackFrameValid(const std::string& text,
                           size_t line,
                           size_t column,
                           const std::string& source,
                           const std::string& function) {
  std::u16string utf16_text = base::UTF8ToUTF16(text);
  std::unique_ptr<StackFrame> frame = StackFrame::CreateFromText(utf16_text);

  ASSERT_TRUE(frame.get()) << "Failed to create frame from '" << text << "'";
  EXPECT_EQ(line, frame->line_number);
  EXPECT_EQ(column, frame->column_number);
  EXPECT_EQ(base::UTF8ToUTF16(source), frame->source);
  EXPECT_EQ(base::UTF8ToUTF16(function), frame->function);
}

void AssertStackFrameInvalid(const std::string& text) {
  std::u16string utf16_text = base::UTF8ToUTF16(text);
  std::unique_ptr<StackFrame> frame = StackFrame::CreateFromText(utf16_text);
  ASSERT_FALSE(frame.get()) << "Errantly created frame from '" << text << "'";
}

}  // namespace

TEST(StackFrameUnitTest, ParseStackFramesFromText) {
  AssertStackFrameValid(
      "function_name (https://www.url.com/foo.html:100:201)",
      100u, 201u, "https://www.url.com/foo.html", "function_name");
  AssertStackFrameValid(
      "(anonymous function) (https://www.url.com/foo.html:100:201)",
      100u, 201u, "https://www.url.com/foo.html", "(anonymous function)");
  AssertStackFrameValid(
      "Function.target.(anonymous function) (internals::SafeBuiltins:19:14)",
      19u, 14u, "internals::SafeBuiltins",
      "Function.target.(anonymous function)");
  AssertStackFrameValid(
      "internal-item:://fpgohbggpmcpeedljibghijiclejiklo/script.js:6:12",
      6u, 12u, "internal-item:://fpgohbggpmcpeedljibghijiclejiklo/script.js",
      "(anonymous function)");

  // No delimiting ':' between line/column numbers.
  AssertStackFrameInvalid(
      "function_name (https://www.url.com/foo.html:100201)");
  // No line number.
  AssertStackFrameInvalid("function_name (https://www.url.com/foo.html::201)");
  // No line number or delimiting ':'.
  AssertStackFrameInvalid("function_name (https://www.url.com/foo.html201)");
  // No leading '(' around url, line, column.
  AssertStackFrameInvalid(
      "function_name https://www.url.com/foo.html:100:201)");
  // No trailing ')'.
  AssertStackFrameInvalid(
      "function_name (https://www.url.com/foo.html:100:201");
  // Trailing ' '.
  AssertStackFrameInvalid(
      "function_name (https://www.url.com/foo.html:100:201) ");
  // Invalid column number.
  AssertStackFrameInvalid(
      "function_name (https://www.url.com/foo.html:100:201a)");
  // Negative column number.
  AssertStackFrameInvalid(
      "function_name (https://www.url.com/foo.html:100:-201)");
  // Extra trailing ')'
  AssertStackFrameInvalid(
      "function_name (https://www.url.com/foo.html:100:201))");
}

}  // namespace extensions
