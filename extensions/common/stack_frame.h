// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_STACK_FRAME_H_
#define EXTENSIONS_COMMON_STACK_FRAME_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>


namespace extensions {

struct StackFrame {
  StackFrame();
  StackFrame(const StackFrame& frame);
  StackFrame(uint32_t line_number,
             uint32_t column_number,
             const std::u16string& source,
             const std::u16string& function);
  ~StackFrame();

  // Construct a stack frame from a reported plain-text frame.
  static std::unique_ptr<StackFrame> CreateFromText(
      const std::u16string& frame_text);

  bool operator==(const StackFrame& rhs) const;

  // Note: we use uint32_t instead of size_t because this struct is sent over
  // IPC which could span 32 & 64 bit processes. This is fine since line numbers
  // and column numbers shouldn't exceed UINT32_MAX even on 64 bit builds.
  uint32_t line_number;
  uint32_t column_number;
  std::u16string source;
  std::u16string function;  // optional
};

using StackTrace = std::vector<StackFrame>;

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_STACK_FRAME_H_

