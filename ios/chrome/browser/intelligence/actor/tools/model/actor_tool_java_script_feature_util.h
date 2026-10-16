// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ACTOR_TOOL_JAVA_SCRIPT_FEATURE_UTIL_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ACTOR_TOOL_JAVA_SCRIPT_FEATURE_UTIL_H_

#import "base/functional/function_ref.h"
#import "components/actor/public/mojom/actor_types.mojom-forward.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"

namespace base {
class ElapsedTimer;
class Value;
}  // namespace base

namespace actor {

// Parses a `ToolExecutionResult` from a JS object that contains a `resultCode`
// number and an optional `message`. The `resultCode` is translated to a
// `mojom::ActionResultCode` with the provided `result_code_translator`.
//
// Also records UMA metrics for `call`:
// - If `result` is null (the call timed out), records
//   `mojom::ActionResultCode::kToolTimeout` to the result code histogram.
//   Execution duration is not recorded.
// - If `result` is a valid dictionary, records the translated result code and
//   `timer.Elapsed()` as the execution duration.
// - If `result` is malformed, records neither metric.
//
// `timer` must be started before `CallJavaScriptFunction()` is invoked.
ToolExecutionResult ParseJavaScriptResultWithResultCode(
    ActorJavaScriptCall call,
    base::FunctionRef<mojom::ActionResultCode(int)> result_code_translator,
    const base::Value* result,
    const base::ElapsedTimer& timer);

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ACTOR_TOOL_JAVA_SCRIPT_FEATURE_UTIL_H_
