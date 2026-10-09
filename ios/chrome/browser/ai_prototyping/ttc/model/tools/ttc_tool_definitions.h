// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TOOLS_TTC_TOOL_DEFINITIONS_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TOOLS_TTC_TOOL_DEFINITIONS_H_

#import <vector>

#import "components/ttc/app/public/tool_types.h"

namespace ttc {

// Canonical navigation and browsing tool names matching Desktop Chrome's
// tools.mojom contract. Listed in strict alphabetical order.
inline constexpr char kToolGoBack[] = "go_back";
inline constexpr char kToolGoForward[] = "go_forward";
inline constexpr char kToolOpenUrl[] = "open_url";

// Returns the vector of `ToolDefinition` declarations for the default
// navigation toolset.
std::vector<ToolDefinition> GetDefaultToolDefinitions();

}  // namespace ttc

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TOOLS_TTC_TOOL_DEFINITIONS_H_
