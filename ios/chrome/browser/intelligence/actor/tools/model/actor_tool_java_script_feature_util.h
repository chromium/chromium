// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ACTOR_TOOL_JAVA_SCRIPT_FEATURE_UTIL_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ACTOR_TOOL_JAVA_SCRIPT_FEATURE_UTIL_H_

#import "base/functional/function_ref.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"

namespace base {
class Value;
}  // namespace base

namespace actor {

// Parses a ToolExecutionResult from a JS object that contains a `resultCode`
// number and `message`. The resultCode is translated to a C++ error code with
// the provided `resultCodeTranslator`.
ToolExecutionResult ParseJavaScriptResultWithResultCode(
    base::FunctionRef<mojom::ActionResultCode(int)> resultCodeTranslator,
    const base::Value* result);

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ACTOR_TOOL_JAVA_SCRIPT_FEATURE_UTIL_H_
