// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/java_script_content_world.h"

#include "base/check_op.h"
#include "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/web/public/browser_state.h"
#include "ios/web/public/js_messaging/java_script_feature.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

namespace {

WKUserScriptInjectionTime InjectionTimeToWKUserScriptInjectionTime(
    JavaScriptFeature::FeatureScript::InjectionTime injection_time) {
  switch (injection_time) {
    case JavaScriptFeature::FeatureScript::InjectionTime::kDocumentStart:
      return WKUserScriptInjectionTimeAtDocumentStart;
    case JavaScriptFeature::FeatureScript::InjectionTime::kDocumentEnd:
      return WKUserScriptInjectionTimeAtDocumentEnd;
  }
  NOTREACHED();
  return WKUserScriptInjectionTimeAtDocumentStart;
}

// Returns the WKUserContentController associated with |browser_state|.
// NOTE: Only fetch the WKUserContentController once at construction. Although
// it is not guaranteed to remain constant over the lifetime of the
// application, the entire JavaScriptcontentWorld will be recreated when it
// changes. Calling WKWebViewConfigurationProvider::GetWebViewConfiguration on
// a configuration provider during destruction will cause partial
// re-initialization during tear down.
WKUserContentController* GetUserContentController(BrowserState* browser_state) {
  return WKWebViewConfigurationProvider::FromBrowserState(browser_state)
      .GetWebViewConfiguration()
      .userContentController;
}

}  // namespace

JavaScriptContentWorld::JavaScriptContentWorld(BrowserState* browser_state)
    : browser_state_(browser_state),
      user_content_controller_(GetUserContentController(browser_state)) {}

#if defined(__IPHONE_14_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_14_0
JavaScriptContentWorld::JavaScriptContentWorld(BrowserState* browser_state,
                                               WKContentWorld* content_world)
    : browser_state_(browser_state),
      user_content_controller_(GetUserContentController(browser_state)),
      content_world_(content_world) {}

WKContentWorld* JavaScriptContentWorld::GetWKContentWorld()
    API_AVAILABLE(ios(14.0)) {
  return content_world_;
}
#endif  // defined(__IPHONE14_0)

JavaScriptContentWorld::~JavaScriptContentWorld() {}

bool JavaScriptContentWorld::HasFeature(const JavaScriptFeature* feature) {
  return features_.find(feature) != features_.end();
}

void JavaScriptContentWorld::AddFeature(const JavaScriptFeature* feature) {
  if (HasFeature(feature)) {
    // |feature| has already been added to this content world.
    return;
  }

#if defined(__IPHONE_14_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_14_0
  if (@available(iOS 14, *)) {
    // Ensure |feature| supports this content world.
    if (content_world_ && content_world_ != WKContentWorld.pageWorld) {
      DCHECK_EQ(feature->GetSupportedContentWorld(),
                JavaScriptFeature::ContentWorld::kAnyContentWorld);
    }
  }
#endif  // defined(__IPHONE14_0)

  features_.insert(feature);

  // Add dependent features first.
  for (const JavaScriptFeature* dep_feature : feature->GetDependentFeatures()) {
    AddFeature(dep_feature);
  }

  // Setup user scripts.
  for (const JavaScriptFeature::FeatureScript& feature_script :
       feature->GetScripts()) {
    WKUserScriptInjectionTime injection_time =
        InjectionTimeToWKUserScriptInjectionTime(
            feature_script.GetInjectionTime());

    bool main_frame_only =
        feature_script.GetTargetFrames() !=
        JavaScriptFeature::FeatureScript::TargetFrames::kAllFrames;

    WKUserScript* user_script = nil;
#if defined(__IPHONE_14_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_14_0
    if (@available(iOS 14, *)) {
      if (content_world_) {
        user_script = [[WKUserScript alloc]
              initWithSource:feature_script.GetScriptString()
               injectionTime:injection_time
            forMainFrameOnly:main_frame_only
              inContentWorld:content_world_];
      }
    }
#endif  // defined(__IPHONE14_0)

    if (!user_script) {
      user_script =
          [[WKUserScript alloc] initWithSource:feature_script.GetScriptString()
                                 injectionTime:injection_time
                              forMainFrameOnly:main_frame_only];
    }

    [user_content_controller_ addUserScript:user_script];
    // Setup Javascript message callbacks.
    for (auto handlers_by_name : feature->GetScriptMessageHandlers()) {
      std::unique_ptr<ScopedWKScriptMessageHandler> script_message_handler;

#if defined(__IPHONE_14_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_14_0
      if (@available(iOS 14, *)) {
        if (content_world_) {
          script_message_handler =
              std::make_unique<ScopedWKScriptMessageHandler>(
                  user_content_controller_,
                  base::SysUTF8ToNSString(handlers_by_name.first),
                  content_world_,
                  base::BindRepeating(handlers_by_name.second, browser_state_));
        }
      }
#endif  // defined(__IPHONE14_0)

      if (!script_message_handler.get()) {
        script_message_handler = std::make_unique<ScopedWKScriptMessageHandler>(
            user_content_controller_,
            base::SysUTF8ToNSString(handlers_by_name.first),
            base::BindRepeating(handlers_by_name.second, browser_state_));
      }
      script_message_handlers_[feature] = std::move(script_message_handler);
    }
  }
}

}  // namespace web
