// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/stack_frame.h"

#include <memory>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "third_party/re2/src/re2/re2.h"

namespace extensions {

namespace {
const char16_t kAnonymousFunction[] = u"(anonymous function)";
}

StackFrame::StackFrame() : line_number(1), column_number(1) {
}

StackFrame::StackFrame(const StackFrame& frame)
    : line_number(frame.line_number),
      column_number(frame.column_number),
      source(frame.source),
      function(frame.function) {
}

StackFrame::StackFrame(uint32_t line_number,
                       uint32_t column_number,
                       const std::u16string& source,
                       const std::u16string& function)
    : line_number(line_number),
      column_number(column_number),
      source(source),
      function(function.empty() ? kAnonymousFunction : function) {}

StackFrame::~StackFrame() {
}

// Create a stack frame from the passed text. The text must follow one of two
// formats:
//   - "function_name (source:line_number:column_number)"
//   - "source:line_number:column_number"
// (We have to recognize two formats because V8 will report stack traces in
// both ways. If we reconcile this, we can clean this up.)
// static
std::unique_ptr<StackFrame> StackFrame::CreateFromText(
    const std::u16string& frame_text) {
  // We need to use utf8 for re2 matching.
  std::string text = base::UTF16ToUTF8(frame_text);

  size_t line = 1;
  size_t column = 1;
  std::string source;
  std::string function;
  if (!re2::RE2::FullMatch(text,
                           "(.+) \\(([^\\(\\)]+):(\\d+):(\\d+)\\)",
                           &function, &source, &line, &column) &&
      !re2::RE2::FullMatch(text,
                           "([^\\(\\)]+):(\\d+):(\\d+)",
                           &source, &line, &column)) {
    return nullptr;
  }

  return std::make_unique<StackFrame>(line, column, base::UTF8ToUTF16(source),
                                      base::UTF8ToUTF16(function));
}

bool StackFrame::operator==(const StackFrame& rhs) const {
  return line_number == rhs.line_number &&
         column_number == rhs.column_number &&
         source == rhs.source &&
         function == rhs.function;
}

}  // namespace extensions
