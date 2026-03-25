// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actuation/model/tools/actuation_java_script_feature_util.h"

#import "base/functional/callback.h"
#import "base/values.h"
#import "ios/chrome/browser/intelligence/actuation/model/actuation_error.h"

void ParseJavaScriptResult(ActuationTool::ActuationCallback callback,
                           const base::Value* result) {
  if (!result || !result->is_dict()) {
    std::move(callback).Run(base::unexpected(ActuationError{
        ActuationErrorCode::kJavascriptFeatureGotInvalidResult}));
    return;
  }
  const base::DictValue& result_dict = result->GetDict();
  bool success = result_dict.FindBool("success").value_or(false);
  if (!success) {
    const std::string* error_message = result_dict.FindString("message");
    std::move(callback).Run(base::unexpected(ActuationError{
        ActuationErrorCode::kJavascriptFeatureFailedInJavaScriptExecution,
        error_message ? *error_message : "Unknown error in JS."}));
    return;
  }
  std::move(callback).Run(base::ok());
}
