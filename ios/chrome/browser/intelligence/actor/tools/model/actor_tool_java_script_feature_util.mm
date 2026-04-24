// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_java_script_feature_util.h"

#import "base/functional/callback.h"
#import "base/values.h"
#import "components/actor/public/mojom/actor_types.mojom.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_error.h"

namespace actor {

void ParseJavaScriptResult(ToolExecutionCallback callback,
                           const base::Value* result) {
  // `result` being null indicates that the JS function call timed out.
  // TODO(crbug.com/505037793): return a timeout error here.
  if (!result || !result->is_dict()) {
    std::move(callback).Run(ToolExecutionResult(
        ActorToolErrorCode::kJavascriptFeatureGotInvalidResult));
    return;
  }
  const base::DictValue& result_dict = result->GetDict();
  bool success = result_dict.FindBool("success").value_or(false);
  if (!success) {
    const std::string* error_message = result_dict.FindString("message");
    std::move(callback).Run(ToolExecutionResult(
        // TODO(crbug.com/505037793): return more tool-specific errors.
        ActorToolErrorCode::kJavascriptFeatureFailedInJavaScriptExecution,
        error_message ? *error_message : "Unknown error in JS."));
    return;
  }
  std::move(callback).Run(ToolExecutionResult::Ok());
}

}  // namespace actor
