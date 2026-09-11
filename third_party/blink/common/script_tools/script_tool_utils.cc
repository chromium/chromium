// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/script_tools/script_tool_utils.h"

#include <algorithm>

#include "base/strings/string_util.h"

namespace blink {

bool IsValidScriptToolName(std::string_view name) {
  if (name.empty() || name.length() > 128) {
    return false;
  }
  return std::ranges::all_of(name, [](char c) {
    return base::IsAsciiAlphaNumeric(c) || c == '_' || c == '-' || c == '.';
  });
}

}  // namespace blink
