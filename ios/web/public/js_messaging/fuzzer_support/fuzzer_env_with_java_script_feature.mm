// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/js_messaging/fuzzer_support/fuzzer_env_with_java_script_feature.h"

#import "ios/web/public/js_messaging/java_script_feature.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

FuzzerEnvWithJavaScriptFeature::FuzzerEnvWithJavaScriptFeature(
    JavaScriptFeature* feature)
    : feature_(feature) {}
void FuzzerEnvWithJavaScriptFeature::InvokeScriptMessageReceived(
    const web::ScriptMessage& message) {
  feature_->ScriptMessageReceived(web_state(), message);
}

}  // namespace web
