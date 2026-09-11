// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_SCRIPT_TOOLS_SCRIPT_TOOL_UTILS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_SCRIPT_TOOLS_SCRIPT_TOOL_UTILS_H_

#include <string_view>

#include "third_party/blink/public/common/common_export.h"

namespace blink {

// Returns true if the given tool name is valid according to the WebMCP spec:
// https://webmachinelearning.github.io/webmcp/#dom-modelcontext-registertool
// Specifically, it must not be empty, must not exceed 128 characters, and must
// contain only ASCII alphanumeric characters, '_', '-', or '.'.
BLINK_COMMON_EXPORT bool IsValidScriptToolName(std::string_view name);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_SCRIPT_TOOLS_SCRIPT_TOOL_UTILS_H_
