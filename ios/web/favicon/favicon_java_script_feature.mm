// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/favicon/favicon_java_script_feature.h"

#import <vector>

#import "base/values.h"
#import "ios/web/favicon/favicon_util.h"
#import "ios/web/public/js_messaging/java_script_feature_util.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/web_state/web_state_impl.h"

namespace {
const char kScriptName[] = "favicon";
const char kEventListenersScriptName[] = "favicon_event_listeners";

const char kFaviconScriptHandlerName[] = "FaviconUrlsHandler";

}  // namespace

namespace web {

FaviconJavaScriptFeature::FaviconJavaScriptFeature()
    : JavaScriptFeature(
          ContentWorld::kIsolatedWorld,
          {FeatureScript::CreateWithFilename(
               kScriptName,
               FeatureScript::InjectionTime::kDocumentEnd,
               FeatureScript::TargetFrames::kMainFrame,
               FeatureScript::ReinjectionBehavior::kInjectOncePerWindow),
           FeatureScript::CreateWithFilename(
               kEventListenersScriptName,
               FeatureScript::InjectionTime::kDocumentEnd,
               FeatureScript::TargetFrames::kMainFrame,
               FeatureScript::ReinjectionBehavior::
                   kReinjectOnDocumentRecreation)},
          {web::java_script_features::GetCommonJavaScriptFeature()}) {}

FaviconJavaScriptFeature::~FaviconJavaScriptFeature() {}

std::optional<std::string>
FaviconJavaScriptFeature::GetScriptMessageHandlerName() const {
  return kFaviconScriptHandlerName;
}

void FaviconJavaScriptFeature::ScriptMessageReceived(
    web::WebState* web_state,
    const web::ScriptMessage& message) {
  DCHECK(message.is_main_frame());

  if (!message.body() || !message.body()->is_list() || !message.request_url()) {
    return;
  }

  const GURL url = message.request_url().value();

  std::vector<FaviconURL> urls;
  if (!ExtractFaviconURL(message.body()->GetList(), url, &urls))
    return;

  if (!urls.empty()) {
    WebStateImpl::FromWebState(web_state)->OnFaviconUrlUpdated(urls);
  }
}

}  // namespace web
