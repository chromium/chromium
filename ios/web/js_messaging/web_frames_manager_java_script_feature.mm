// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/web_frames_manager_java_script_feature.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/web/js_messaging/java_script_content_world_util.h"
#import "ios/web/js_messaging/java_script_feature_manager.h"
#import "ios/web/js_messaging/web_frame_impl.h"
#import "ios/web/js_messaging/web_frames_manager_impl.h"
#import "ios/web/js_messaging/web_view_web_state_map.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/js_messaging/java_script_feature_util.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/web_state/web_state_impl.h"
#import "ios/web/web_view/wk_security_origin_util.h"

namespace web {

namespace {
const char kWebFramesManagerJavaScriptFeatureContainerKeyName[] =
    "web_frames_manager_java_script_feature_container";

const char kSetupFrameScriptName[] = "setup_frame";
const char kFrameListenersScriptName[] = "frame_listeners";

// Message handler called when a frame becomes available.
NSString* const kFrameAvailableScriptHandlerName = @"FrameBecameAvailable";
// Message handler called when a frame is unloading.
NSString* const kFrameUnavailableScriptHandlerName = @"FrameBecameUnavailable";

}  // namespace

#pragma mark - WebFramesManagerJavaScriptFeature::Container

// static
WebFramesManagerJavaScriptFeature::Container*
WebFramesManagerJavaScriptFeature::Container::FromBrowserState(
    BrowserState* browser_state) {
  DCHECK(browser_state);

  WebFramesManagerJavaScriptFeature::Container* container =
      static_cast<WebFramesManagerJavaScriptFeature::Container*>(
          browser_state->GetUserData(
              kWebFramesManagerJavaScriptFeatureContainerKeyName));
  if (!container) {
    container = new WebFramesManagerJavaScriptFeature::Container(browser_state);
    browser_state->SetUserData(
        kWebFramesManagerJavaScriptFeatureContainerKeyName,
        base::WrapUnique(container));
  }
  return container;
}

WebFramesManagerJavaScriptFeature::Container::Container(
    BrowserState* browser_state)
    : browser_state_(browser_state) {}
WebFramesManagerJavaScriptFeature::Container::~Container() = default;

WebFramesManagerJavaScriptFeature*
WebFramesManagerJavaScriptFeature::Container::FeatureForContentWorld(
    ContentWorld content_world) {
  DCHECK_NE(content_world, ContentWorld::kAllContentWorlds);

  auto& feature = features_[content_world];
  if (!feature) {
    feature = base::WrapUnique(
        new WebFramesManagerJavaScriptFeature(content_world, browser_state_));
  }
  return feature.get();
}

#pragma mark - WebFramesManagerJavaScriptFeature

WebFramesManagerJavaScriptFeature::WebFramesManagerJavaScriptFeature(
    ContentWorld content_world,
    BrowserState* browser_state)
    : JavaScriptFeature(
          ContentWorld::kAllContentWorlds,
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
      content_world_(content_world),
      browser_state_(browser_state),
      weak_factory_(this) {}

WebFramesManagerJavaScriptFeature::~WebFramesManagerJavaScriptFeature() =
    default;

// static
std::vector<WebFramesManagerJavaScriptFeature*>
WebFramesManagerJavaScriptFeature::AllContentWorldFeaturesFromBrowserState(
    BrowserState* browser_state) {
  JavaScriptFeatureManager* feature_manager =
      JavaScriptFeatureManager::FromBrowserState(browser_state);

  std::vector<WebFramesManagerJavaScriptFeature*> features;
  for (ContentWorld world : feature_manager->GetAllContentWorldEnums()) {
    features.push_back(
        WebFramesManagerJavaScriptFeature::Container::FromBrowserState(
            browser_state)
            ->FeatureForContentWorld(world));
  }
  return features;
}

void WebFramesManagerJavaScriptFeature::ConfigureHandlers(
    WKUserContentController* user_content_controller) {
  // Reset the old handlers first as handlers with the same name can not be
  // added simultaneously.
  frame_available_handler_.reset();
  frame_unavailable_handler_.reset();

  WKContentWorld* wk_content_world =
      WKContentWorldForContentWorldIdentifier(content_world_);

  frame_available_handler_ = std::make_unique<ScopedWKScriptMessageHandler>(
      user_content_controller, kFrameAvailableScriptHandlerName,
      wk_content_world,
      base::BindRepeating(
          &WebFramesManagerJavaScriptFeature::FrameAvailableMessageReceived,
          weak_factory_.GetWeakPtr()));

  frame_unavailable_handler_ = std::make_unique<ScopedWKScriptMessageHandler>(
      user_content_controller, kFrameUnavailableScriptHandlerName,
      wk_content_world,
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

  WebFramesManagerImpl& web_frames_manager =
      WebStateImpl::FromWebState(web_state)->GetWebFramesManagerImpl(
          content_world_);

  std::string frame_id = base::SysNSStringToUTF8(message.body[@"crwFrameId"]);
  if (web_frames_manager.GetFrameWithId(frame_id)) {
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

  web_frames_manager.AddFrame(std::move(new_frame));
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

  WebFramesManagerImpl& web_frames_manager =
      WebStateImpl::FromWebState(web_state)->GetWebFramesManagerImpl(
          content_world_);
  web_frames_manager.RemoveFrameWithId(frame_id);
}

}  // namespace web
