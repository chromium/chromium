// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/model/aim_cobrowse_java_script_feature.h"

#import "base/base64.h"
#import "base/strings/utf_string_conversions.h"
#import "base/values.h"
#import "components/search_engines/util.h"
#import "ios/chrome/browser/cobrowse/model/assistant_aim_tab_helper.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"
#import "third_party/lens_server_proto/aim_communication.pb.h"

namespace {
const char kScriptName[] = "aim_cobrowse";
}  // namespace

// static
AimCobrowseJavaScriptFeature* AimCobrowseJavaScriptFeature::GetInstance() {
  static base::NoDestructor<AimCobrowseJavaScriptFeature> instance;
  return instance.get();
}

AimCobrowseJavaScriptFeature::AimCobrowseJavaScriptFeature()
    : web::JavaScriptFeature(
          web::ContentWorld::kPageContentWorld,
          {FeatureScript::CreateWithFilename(
              kScriptName,
              FeatureScript::InjectionTime::kDocumentStart,
              FeatureScript::TargetFrames::kMainFrame,
              FeatureScript::ReinjectionBehavior::kInjectOncePerWindow,
              FeatureScript::PlaceholderReplacementsCallback(),
              web::OriginFilter::kGoogleSearch)},
          {},
          web::OriginFilter::kGoogleSearch) {}

AimCobrowseJavaScriptFeature::~AimCobrowseJavaScriptFeature() = default;

void AimCobrowseJavaScriptFeature::SendNativeToWeb(
    web::WebState* web_state,
    const lens::ClientToAimMessage& message) {
  if (!web_state) {
    return;
  }
  web::WebFrame* main_frame = GetWebFramesManager(web_state)->GetMainWebFrame();
  if (!main_frame) {
    return;
  }

  std::string serialized_message;
  message.SerializeToString(&serialized_message);
  std::string base64_message = base::Base64Encode(serialized_message);

  base::ListValue parameters;
  parameters.Append(base64_message);

  CallJavaScriptFunction(main_frame, "aimCobrowse.sendNativeToWeb", parameters);
}

std::optional<std::string>
AimCobrowseJavaScriptFeature::GetScriptMessageHandlerName() const {
  return "AimCobrowseMessageHandler";
}

void AimCobrowseJavaScriptFeature::ScriptMessageReceived(
    web::WebState* web_state,
    const web::ScriptMessage& message) {
  AssistantAimTabHelper* tab_helper =
      AssistantAimTabHelper::FromWebState(web_state);
  if (!tab_helper) {
    return;
  }
  if (!message.is_main_frame()) {
    return;
  }
  std::optional<GURL> request_url = message.request_url();
  if (!request_url ||
      (!IsAimURL(*request_url) && !IsAimZeroStateURL(*request_url))) {
    return;
  }
  if (!message.body() || !message.body()->is_dict()) {
    return;
  }
  const base::DictValue& dict = message.body()->GetDict();
  const std::string* base64_message = dict.FindString("message");
  if (!base64_message) {
    return;
  }
  std::string serialized_message;
  if (!base::Base64Decode(*base64_message, &serialized_message)) {
    return;
  }
  lens::AimToClientMessage aim_to_client_message;
  if (!aim_to_client_message.ParseFromString(serialized_message)) {
    return;
  }
  tab_helper->OnMessageReceived(aim_to_client_message);
}
