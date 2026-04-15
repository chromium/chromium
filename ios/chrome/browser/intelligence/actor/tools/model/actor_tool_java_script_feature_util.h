// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ACTOR_TOOL_JAVA_SCRIPT_FEATURE_UTIL_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ACTOR_TOOL_JAVA_SCRIPT_FEATURE_UTIL_H_

#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"

namespace base {
class Value;
}  // namespace base

namespace actor {

// Processes a standard {success: bool, message: string} JS result.
void ParseJavaScriptResult(ToolExecutionCallback callback,
                           const base::Value* result);

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ACTOR_TOOL_JAVA_SCRIPT_FEATURE_UTIL_H_
