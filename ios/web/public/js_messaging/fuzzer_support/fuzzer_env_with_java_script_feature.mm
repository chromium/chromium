// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/js_messaging/fuzzer_support/fuzzer_env_with_java_script_feature.h"

#import "ios/web/public/js_messaging/java_script_feature.h"

namespace web {

FuzzerEnvWithJavaScriptFeature::FuzzerEnvWithJavaScriptFeature(
    JavaScriptFeature* feature)
    : feature_(feature) {}
void FuzzerEnvWithJavaScriptFeature::InvokeScriptMessageReceived(
    const web::ScriptMessage& message) {
  feature_->ScriptMessageReceived(web_state(), message);
}

}  // namespace web
