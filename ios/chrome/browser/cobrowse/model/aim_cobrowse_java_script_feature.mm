// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/model/aim_cobrowse_java_script_feature.h"

#import "base/base64.h"
#import "base/strings/utf_string_conversions.h"
#import "base/values.h"
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
          web::ContentWorld::kIsolatedWorld,
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

void AimCobrowseJavaScriptFeature::PostMessage(
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

  CallJavaScriptFunction(main_frame, "aimCobrowse.postMessage", parameters);
}
