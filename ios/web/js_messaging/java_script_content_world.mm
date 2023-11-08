// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/java_script_content_world.h"

#import <optional>

#import "base/check_op.h"
#import "base/containers/contains.h"
#import "base/debug/crash_logging.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/web/js_messaging/web_view_js_utils.h"
#import "ios/web/js_messaging/web_view_web_state_map.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/js_messaging/content_world.h"
#import "ios/web/public/js_messaging/java_script_feature.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"
#import "ios/web/web_state/web_state_impl.h"
#import "net/base/mac/url_conversions.h"

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

// Returns the WKUserContentController associated with `browser_state`.
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

JavaScriptContentWorld::JavaScriptContentWorld(BrowserState* browser_state,
                                               WKContentWorld* content_world)
    : browser_state_(browser_state),
      user_content_controller_(GetUserContentController(browser_state)),
      content_world_(content_world),
      weak_factory_(this) {
  DCHECK(content_world_);
}

WKContentWorld* JavaScriptContentWorld::GetWKContentWorld() {
  return content_world_;
}

JavaScriptContentWorld::~JavaScriptContentWorld() {}

bool JavaScriptContentWorld::HasFeature(const JavaScriptFeature* feature) {
  return base::Contains(features_, feature);
}

void JavaScriptContentWorld::AddFeature(const JavaScriptFeature* feature) {
  if (HasFeature(feature)) {
    // `feature` has already been added to this content world.
    return;
  }

  // Ensure `feature` supports this `content_world_`.
  if (content_world_ == WKContentWorld.pageWorld) {
    // A feature specifying kIsolatedWorld can not be added to the page
    // content world.
    DCHECK_NE(feature->GetSupportedContentWorld(),
              ContentWorld::kIsolatedWorld);
  } else {
    // A feature specifying kPageContentWorld can not be added to an isolated
    // world.
    DCHECK_NE(feature->GetSupportedContentWorld(),
              ContentWorld::kPageContentWorld);
  }

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

    if (content_world_) {
      WKUserScript* user_script =
          [[WKUserScript alloc] initWithSource:feature_script.GetScriptString()
                                 injectionTime:injection_time
                              forMainFrameOnly:main_frame_only
                                inContentWorld:content_world_];
      [user_content_controller_ addUserScript:user_script];
    } else {
      WKUserScript* user_script =
          [[WKUserScript alloc] initWithSource:feature_script.GetScriptString()
                                 injectionTime:injection_time
                              forMainFrameOnly:main_frame_only];
      [user_content_controller_ addUserScript:user_script];
    }
  }

  // Setup Javascript message callback.
  auto optional_handler_name = feature->GetScriptMessageHandlerName();
  if (optional_handler_name) {
    auto handler = feature->GetScriptMessageHandler();
    DCHECK(handler);

    NSString* handler_name =
        base::SysUTF8ToNSString(optional_handler_name.value());

    std::unique_ptr<ScopedWKScriptMessageHandler> script_message_handler;
    if (content_world_) {
      script_message_handler = std::make_unique<ScopedWKScriptMessageHandler>(
          user_content_controller_, handler_name, content_world_,
          base::BindRepeating(&JavaScriptContentWorld::ScriptMessageReceived,
                              weak_factory_.GetWeakPtr(), handler.value(),
                              browser_state_));
    } else {
      script_message_handler = std::make_unique<ScopedWKScriptMessageHandler>(
          user_content_controller_, handler_name,
          base::BindRepeating(&JavaScriptContentWorld::ScriptMessageReceived,
                              weak_factory_.GetWeakPtr(), handler.value(),
                              browser_state_));
    }
    script_message_handlers_[feature] = std::move(script_message_handler);
  }
}

void JavaScriptContentWorld::ScriptMessageReceived(
    JavaScriptFeature::ScriptMessageHandler handler,
    BrowserState* browser_state,
    WKScriptMessage* script_message) {
  SCOPED_CRASH_KEY_STRING32("ScriptMessage", "name",
                            base::SysNSStringToUTF8(script_message.name));

  web::WebViewWebStateMap* map =
      web::WebViewWebStateMap::FromBrowserState(browser_state);
  web::WebState* web_state = map->GetWebStateForWebView(script_message.webView);

  // Drop messages if they are no longer associated with a WebState.
  if (!web_state) {
    return;
  }

  CRWWebController* web_controller =
      web::WebStateImpl::FromWebState(web_state)->GetWebController();
  if (!web_controller) {
    return;
  }

  NSURL* ns_url = script_message.frameInfo.request.URL;
  std::optional<GURL> url;
  if (ns_url) {
    url = net::GURLWithNSURL(ns_url);
  }

  ScriptMessage message(web::ValueResultFromWKResult(script_message.body),
                        web_controller.isUserInteracting,
                        script_message.frameInfo.mainFrame, url);

  handler.Run(web_state, message);
}

}  // namespace web
