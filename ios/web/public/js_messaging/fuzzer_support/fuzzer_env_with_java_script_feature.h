// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_JS_MESSAGING_FUZZER_SUPPORT_FUZZER_ENV_WITH_JAVA_SCRIPT_FEATURE_H_
#define IOS_WEB_PUBLIC_JS_MESSAGING_FUZZER_SUPPORT_FUZZER_ENV_WITH_JAVA_SCRIPT_FEATURE_H_

#import "base/memory/raw_ptr.h"
#import "ios/web/public/js_messaging/java_script_feature.h"
#include "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/test/fuzzer_env_with_web_state.h"

namespace web {

// This class initializes web task environment through it's parent class's
// constructor and stores pointers to WebState & JavaScriptFeature instances.
// Fuzzers can invoke private/protected `JavaScriptFeature` APIs through public
// APIs of this class to fuzz these. The class is designed to be used as a
// static variable in fuzzer functions like `LLVMFuzzerTestOneInput`.
class FuzzerEnvWithJavaScriptFeature : public FuzzerEnvWithWebState {
 public:
  FuzzerEnvWithJavaScriptFeature(JavaScriptFeature* feature);

  // Invokes `JavaScriptFeature::ScriptMessageReceived` function with the web
  // state and feature stored in class.
  void InvokeScriptMessageReceived(const web::ScriptMessage& message);

 private:
  raw_ptr<JavaScriptFeature> feature_;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_JS_MESSAGING_FUZZER_SUPPORT_FUZZER_ENV_WITH_JAVA_SCRIPT_FEATURE_H_
