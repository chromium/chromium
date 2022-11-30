// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/web_frames_manager_java_script_feature.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/web/js_messaging/web_frame_impl.h"
#import "ios/web/js_messaging/web_view_web_state_map.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/js_messaging/java_script_feature_util.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frame_util.h"
#import "ios/web/web_state/web_state_impl.h"
#import "ios/web/web_view/wk_security_origin_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

namespace {
const char kWebFramesManagerJavaScriptFeatureKeyName[] =
    "web_frames_manager_java_script_feature";

const char kSetupFrameScriptName[] = "setup_frame";
const char kFrameListenersScriptName[] = "frame_listeners";

// Message handler called when a frame becomes available.
NSString* const kFrameAvailableScriptHandlerName = @"FrameBecameAvailable";
// Message handler called when a frame is unloading.
NSString* const kFrameUnavailableScriptHandlerName = @"FrameBecameUnavailable";

}  // namespace

WebFramesManagerJavaScriptFeature::WebFramesManagerJavaScriptFeature(
    BrowserState* browser_state)
    : JavaScriptFeature(
          ContentWorld::kPageContentWorld,
          {FeatureScript::CreateWithFilename(
               kSetupFrameScriptName,
               FeatureScript::InjectionTime::kDocumentEnd,
               FeatureScript::TargetFrames::kAllFrames,
               FeatureScript::ReinjectionBehavior::kInjectOncePerWindow),
           FeatureScript::CreateWithFilename(
               kFrameListenersScriptName,
               FeatureScript::InjectionTime::kDocumentStart,
               FeatureScript::TargetFrames::kAllFrames,
               FeatureScript::ReinjectionBehavior::
                   kReinjectOnDocumentRecreation)},
          {java_script_features::GetCommonJavaScriptFeature(),
           java_script_features::GetMessageJavaScriptFeature()}),
      browser_state_(browser_state),
      weak_factory_(this) {}

WebFramesManagerJavaScriptFeature::~WebFramesManagerJavaScriptFeature() =
    default;

// static
WebFramesManagerJavaScriptFeature*
WebFramesManagerJavaScriptFeature::FromBrowserState(
    BrowserState* browser_state) {
  DCHECK(browser_state);

  WebFramesManagerJavaScriptFeature* feature =
      static_cast<WebFramesManagerJavaScriptFeature*>(
          browser_state->GetUserData(
              kWebFramesManagerJavaScriptFeatureKeyName));
  if (!feature) {
    feature = new WebFramesManagerJavaScriptFeature(browser_state);
    browser_state->SetUserData(kWebFramesManagerJavaScriptFeatureKeyName,
                               base::WrapUnique(feature));
  }
  return feature;
}

void WebFramesManagerJavaScriptFeature::ConfigureHandlers(
    WKUserContentController* user_content_controller) {
  // Reset the old handlers first as handlers with the same name can not be
  // added simultaneously.
  frame_available_handler_.reset();
  frame_unavailable_handler_.reset();

  frame_available_handler_ = std::make_unique<ScopedWKScriptMessageHandler>(
      user_content_controller, kFrameAvailableScriptHandlerName,
      base::BindRepeating(
          &WebFramesManagerJavaScriptFeature::FrameAvailableMessageReceived,
          weak_factory_.GetWeakPtr()));

  frame_unavailable_handler_ = std::make_unique<ScopedWKScriptMessageHandler>(
      user_content_controller, kFrameUnavailableScriptHandlerName,
      base::BindRepeating(
          &WebFramesManagerJavaScriptFeature::FrameUnavailableMessageReceived,
          weak_factory_.GetWeakPtr()));
}

void WebFramesManagerJavaScriptFeature::FrameAvailableMessageReceived(
    WKScriptMessage* message) {
  WebState* web_state = WebViewWebStateMap::FromBrowserState(browser_state_)
                            ->GetWebStateForWebView(message.webView);
  if (!web_state) {
    // Ignore this message if `message.webView` is no longer associated with a
    // WebState.
    return;
  }

  DCHECK(!web_state->IsBeingDestroyed());

  // Validate all expected message components because any frame could falsify
  // this message.
  if (![message.body isKindOfClass:[NSDictionary class]] ||
      ![message.body[@"crwFrameId"] isKindOfClass:[NSString class]]) {
    return;
  }

  std::string frame_id = base::SysNSStringToUTF8(message.body[@"crwFrameId"]);
  if (web_state->GetWebFramesManager()->GetFrameWithId(frame_id)) {
    return;
  }

  // Validate `frame_id` is a proper hex string.
  for (const char& c : frame_id) {
    if (!base::IsHexDigit(c)) {
      // Ignore frame if `frame_id` is malformed.
      return;
    }
  }

  GURL message_frame_origin =
      web::GURLOriginWithWKSecurityOrigin(message.frameInfo.securityOrigin);

  auto new_frame = std::make_unique<web::WebFrameImpl>(
      message.frameInfo, frame_id, message.frameInfo.mainFrame,
      message_frame_origin, web_state);

  static_cast<web::WebStateImpl*>(web_state)->WebFrameBecameAvailable(
      std::move(new_frame));
}

void WebFramesManagerJavaScriptFeature::FrameUnavailableMessageReceived(
    WKScriptMessage* message) {
  WebState* web_state = WebViewWebStateMap::FromBrowserState(browser_state_)
                            ->GetWebStateForWebView(message.webView);
  if (!web_state) {
    // Ignore this message if `message.webView` is no longer associated with a
    // WebState.
    return;
  }

  DCHECK(!web_state->IsBeingDestroyed());

  if (![message.body isKindOfClass:[NSString class]]) {
    // WebController is being destroyed or message is invalid.
    return;
  }
  std::string frame_id = base::SysNSStringToUTF8(message.body);
  static_cast<web::WebStateImpl*>(web_state)->WebFrameBecameUnavailable(
      frame_id);
}

}  // namespace web
