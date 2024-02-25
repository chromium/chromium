// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/base64.h"
#import "base/rand_util.h"
#import "ios/chrome/browser/web/model/image_fetch/image_fetch_java_script_feature.h"
#import "ios/web/public/js_messaging/fuzzer_support/fuzzer_env_with_java_script_feature.h"
#import "ios/web/public/js_messaging/fuzzer_support/fuzzer_util.h"
#import "ios/web/public/js_messaging/fuzzer_support/js_message.pb.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "testing/libfuzzer/proto/lpm_interface.h"

namespace {

protobuf_mutator::protobuf::LogSilencer log_silencer;

}  // namespace

DEFINE_PROTO_FUZZER(const web::ScriptMessageProto& proto_js_message) {
  static web::FuzzerEnvWithJavaScriptFeature env(
      ImageFetchJavaScriptFeature::GetInstance());
  std::unique_ptr<web::ScriptMessage> script_message =
      web::fuzzer::ProtoToScriptMessage(proto_js_message);
  if (script_message->body() && script_message->body()->is_dict()) {
    // At 20% rate, ensure data field is a encoded string to avoid early return.
    if (base::RandDouble() < 0.2) {
      std::string encoded = base::Base64Encode("some raw data");
      script_message->body()->GetDict().Set("data", encoded);
    }
  }
  env.InvokeScriptMessageReceived(*script_message);
}
