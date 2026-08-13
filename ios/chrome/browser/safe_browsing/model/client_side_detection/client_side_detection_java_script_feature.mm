// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/client_side_detection/client_side_detection_java_script_feature.h"

#import "base/no_destructor.h"
#import "base/values.h"
#import "ios/web/public/js_messaging/script_message.h"

namespace {
constexpr char kScriptFilename[] = "client_side_detection";
constexpr char kMessageHandlerName[] = "ClientSideDetectionMessage";

// Key in the script message dictionary containing the copied text payload.
constexpr char kCopyTextKey[] = "copyText";
}  // namespace

ClientSideDetectionJavaScriptFeature::ClientSideDetectionJavaScriptFeature()
    : JavaScriptFeature(web::ContentWorld::kIsolatedWorld,
                        {FeatureScript::CreateWithFilename(
                            kScriptFilename,
                            FeatureScript::InjectionTime::kDocumentStart,
                            FeatureScript::TargetFrames::kAllFrames,
                            FeatureScript::ReinjectionBehavior::
                                kReinjectOnDocumentRecreation)},
                        {}) {}

ClientSideDetectionJavaScriptFeature::~ClientSideDetectionJavaScriptFeature() =
    default;

// static
ClientSideDetectionJavaScriptFeature*
ClientSideDetectionJavaScriptFeature::GetInstance() {
  static base::NoDestructor<ClientSideDetectionJavaScriptFeature> instance;
  return instance.get();
}

std::optional<std::string>
ClientSideDetectionJavaScriptFeature::GetScriptMessageHandlerName() const {
  return kMessageHandlerName;
}

void ClientSideDetectionJavaScriptFeature::ScriptMessageReceived(
    web::WebState* web_state,
    const web::ScriptMessage& message) {
  if (!message.legacy_body()) {
    return;
  }
  const base::DictValue* dict = message.legacy_body()->GetIfDict();
  if (!dict) {
    return;
  }

  const std::string* copy_text = dict->FindString(kCopyTextKey);
  if (!copy_text || copy_text->empty()) {
    return;
  }

  // TODO(crbug.com/502615476): Hook up forwarding to
  // `ClientSideDetectionHostIOS` when it is introduced.
}
