// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_features/fullscreen/fullscreen_java_script_feature.h"

#import "ios/web/public/js_messaging/java_script_feature_util.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#import "ios/web/web_state/web_state_impl.h"

namespace {
const char kScriptName[] = "fullscreen";

const char kViewportConfigurationHandlerName[] = "FullscreenViewportHandler";

static const char kScriptMessageViewportFitCoverKey[] = "cover";
}  // namespace

namespace web {

// static
FullscreenJavaScriptFeature* FullscreenJavaScriptFeature::GetInstance() {
  static base::NoDestructor<FullscreenJavaScriptFeature> instance;
  return instance.get();
}

FullscreenJavaScriptFeature::FullscreenJavaScriptFeature()
    : JavaScriptFeature(
          ContentWorld::kIsolatedWorld,
          {FeatureScript::CreateWithFilename(
              kScriptName,
              FeatureScript::InjectionTime::kDocumentEnd,
              FeatureScript::TargetFrames::kMainFrame,
              FeatureScript::ReinjectionBehavior::
                  kReinjectOnDocumentRecreation)},
          {web::java_script_features::GetCommonJavaScriptFeature(),
           web::java_script_features::GetMessageJavaScriptFeature()}) {}
FullscreenJavaScriptFeature::~FullscreenJavaScriptFeature() = default;

std::optional<std::string>
FullscreenJavaScriptFeature::GetScriptMessageHandlerName() const {
  return kViewportConfigurationHandlerName;
}

void FullscreenJavaScriptFeature::ScriptMessageReceived(
    WebState* web_state,
    const ScriptMessage& script_message) {
  const base::Value::Dict* script_dict =
      script_message.body() ? script_message.body()->GetIfDict() : nullptr;
  if (!script_dict) {
    return;
  }

  if (!script_message.is_main_frame()) {
    return;
  }

  const std::string* frame_id = script_dict->FindString("frame_id");
  if (!frame_id) {
    return;
  }

  WebFrame* main_frame = GetWebFramesManager(web_state)->GetMainWebFrame();
  std::string main_frame_id = main_frame ? main_frame->GetFrameId() : "";
  if (main_frame_id != *frame_id) {
    // Frame has changed, do not send message to the web controller as it would
    // update the incorrect navigation item.
    return;
  }
  auto cover = script_dict->FindBool(kScriptMessageViewportFitCoverKey);
  if (cover.has_value()) {
    CRWWebController* web_controller =
        WebStateImpl::FromWebState(web_state)->GetWebController();
    [web_controller handleViewportFit:static_cast<BOOL>(cover.value())];
  }
}

}  // namespace web
