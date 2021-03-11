// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_STACK_FRAME_H_
#define EXTENSIONS_COMMON_STACK_FRAME_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/strings/string16.h"

namespace extensions {

struct StackFrame {
  StackFrame();
  StackFrame(const StackFrame& frame);
  StackFrame(uint32_t line_number,
             uint32_t column_number,
             const base::string16& source,
             const base::string16& function);
  ~StackFrame();

  // Construct a stack frame from a reported plain-text frame.
  static std::unique_ptr<StackFrame> CreateFromText(
      const base::string16& frame_text);

  bool operator==(const StackFrame& rhs) const;

  // Note: we use uint32_t instead of size_t because this struct is sent over
  // IPC which could span 32 & 64 bit processes. This is fine since line numbers
  // and column numbers shouldn't exceed UINT32_MAX even on 64 bit builds.
  uint32_t line_number;
  uint32_t column_number;
  base::string16 source;
  base::string16 function;  // optional
};

typedef std::vector<StackFrame> StackTrace;

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_STACK_FRAME_H_

