// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/navigation_java_script_feature.h"

#import "base/no_destructor.h"
#import "ios/web/public/js_messaging/java_script_feature_util.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#import "ios/web/web_state/web_state_impl.h"

namespace web {

namespace {

const char kScriptName[] = "navigation";
const char kListenersScriptName[] = "navigation_listeners";
const char kScriptHandlerName[] = "NavigationEventMessage";

}  // namespace

// static
NavigationJavaScriptFeature* NavigationJavaScriptFeature::GetInstance() {
  static base::NoDestructor<NavigationJavaScriptFeature> instance;
  return instance.get();
}

NavigationJavaScriptFeature::NavigationJavaScriptFeature()
    : JavaScriptFeature(
          ContentWorld::kPageContentWorld,
          {FeatureScript::CreateWithFilename(
               kScriptName,
               FeatureScript::InjectionTime::kDocumentStart,
               FeatureScript::TargetFrames::kMainFrame,
               FeatureScript::ReinjectionBehavior::kInjectOncePerWindow),
           FeatureScript::CreateWithFilename(
               kListenersScriptName,
               FeatureScript::InjectionTime::kDocumentStart,
               FeatureScript::TargetFrames::kMainFrame,
               FeatureScript::ReinjectionBehavior::
                   kReinjectOnDocumentRecreation)},
          {web::java_script_features::GetCommonJavaScriptFeature(),
           web::java_script_features::GetMessageJavaScriptFeature()}) {}

NavigationJavaScriptFeature::~NavigationJavaScriptFeature() = default;

std::optional<std::string>
NavigationJavaScriptFeature::GetScriptMessageHandlerName() const {
  return kScriptHandlerName;
}

void NavigationJavaScriptFeature::ScriptMessageReceived(
    web::WebState* web_state,
    const web::ScriptMessage& message) {
  if (!message.body()) {
    // Ignore malformed responses.
    return;
  }
  auto* dict = message.body()->GetIfDict();
  if (!dict) {
    // Ignore malformed responses.
    return;
  }

  if (!message.is_main_frame()) {
    return;
  }

  const std::string* command = dict->FindString("command");
  if (!command) {
    return;
  }

  const std::string* frame_id = dict->FindString("frame_id");
  if (!frame_id) {
    return;
  }

  WebFrame* main_frame =
      web_state->GetPageWorldWebFramesManager()->GetMainWebFrame();
  std::string main_frame_id = main_frame ? main_frame->GetFrameId() : "";
  if (main_frame_id != *frame_id) {
    // Frame has changed, do not send message to the web controller as it would
    // update the incorrect navigation item.
    return;
  }

  CRWWebController* web_controller =
      WebStateImpl::FromWebState(web_state)->GetWebController();

  if (*command == "hashchange") {
    [web_controller handleNavigationHashChange];
  } else if (*command == "willChangeState") {
    [web_controller handleNavigationWillChangeState];
  } else if (*command == "didPushState") {
    [web_controller handleNavigationDidPushStateMessage:dict];
  } else if (*command == "didReplaceState") {
    [web_controller handleNavigationDidReplaceStateMessage:dict];
  }
}

}  // namespace web
