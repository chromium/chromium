// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/web_frames_manager_java_script_feature.h"

#include "base/base64.h"
#import "base/strings/sys_string_conversions.h"
#include "crypto/symmetric_key.h"
#include "ios/web/js_messaging/web_frame_impl.h"
#import "ios/web/js_messaging/web_view_web_state_map.h"
#include "ios/web/public/browser_state.h"
#import "ios/web/public/js_messaging/java_script_feature_util.h"
#include "ios/web/public/js_messaging/web_frame.h"
#include "ios/web/public/js_messaging/web_frame_util.h"
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

void WebFramesManagerJavaScriptFeature::RegisterExistingFrames(
    WebState* web_state) {
  // This call must be sent to the webstate directly, because the result of this
  // call will create the WebFrames. (Thus, the WebFrames do not yet exist to
  // call into JavaScript at this point.)
  web_state->ExecuteJavaScript(u"__gCrWeb.message.getExistingFrames();");
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
    // Ignore this message if |message.webView| is no longer associated with a
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

  // Validate |frame_id| is a proper hex string.
  for (const char& c : frame_id) {
    if (!base::IsHexDigit(c)) {
      // Ignore frame if |frame_id| is malformed.
      return;
    }
  }

  GURL message_frame_origin =
      web::GURLOriginWithWKSecurityOrigin(message.frameInfo.securityOrigin);

  std::unique_ptr<crypto::SymmetricKey> frame_key;
  if ([message.body[@"crwFrameKey"] isKindOfClass:[NSString class]] &&
      [message.body[@"crwFrameKey"] length] > 0) {
    std::string decoded_frame_key_string;
    std::string encoded_frame_key_string =
        base::SysNSStringToUTF8(message.body[@"crwFrameKey"]);
    base::Base64Decode(encoded_frame_key_string, &decoded_frame_key_string);
    frame_key = crypto::SymmetricKey::Import(
        crypto::SymmetricKey::Algorithm::AES, decoded_frame_key_string);
  }

  auto new_frame = std::make_unique<web::WebFrameImpl>(
      message.frameInfo, frame_id, message.frameInfo.mainFrame,
      message_frame_origin, web_state);
  if (frame_key) {
    new_frame->SetEncryptionKey(std::move(frame_key));
  }

  NSNumber* last_sent_message_id =
      message.body[@"crwFrameLastReceivedMessageId"];
  if ([last_sent_message_id isKindOfClass:[NSNumber class]]) {
    int next_message_id = std::max(0, last_sent_message_id.intValue + 1);
    new_frame->SetNextMessageId(next_message_id);
  }

  static_cast<web::WebStateImpl*>(web_state)->WebFrameBecameAvailable(
      std::move(new_frame));
}

void WebFramesManagerJavaScriptFeature::FrameUnavailableMessageReceived(
    WKScriptMessage* message) {
  WebState* web_state = WebViewWebStateMap::FromBrowserState(browser_state_)
                            ->GetWebStateForWebView(message.webView);
  if (!web_state) {
    // Ignore this message if |message.webView| is no longer associated with a
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
