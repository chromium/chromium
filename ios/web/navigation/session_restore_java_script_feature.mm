// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/session_restore_java_script_feature.h"

#import "ios/web/js_messaging/web_view_js_utils.h"
#import "ios/web/js_messaging/web_view_web_state_map.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"

namespace web {

namespace {

// Key for storing feature on the associated BrowserState.
const char kSessionRestoreJavaScriptFeatureKeyName[] =
    "session_restore_java_script_feature";

// Script message name for session restore.
NSString* const kSessionRestoreScriptHandlerName = @"session_restore";

}  // namespace

// static
SessionRestoreJavaScriptFeature*
SessionRestoreJavaScriptFeature::FromBrowserState(BrowserState* browser_state) {
  DCHECK(browser_state);

  SessionRestoreJavaScriptFeature* feature =
      static_cast<SessionRestoreJavaScriptFeature*>(
          browser_state->GetUserData(kSessionRestoreJavaScriptFeatureKeyName));
  if (!feature) {
    feature = new SessionRestoreJavaScriptFeature(browser_state);
    browser_state->SetUserData(kSessionRestoreJavaScriptFeatureKeyName,
                               base::WrapUnique(feature));
  }
  return feature;
}

SessionRestoreJavaScriptFeature::SessionRestoreJavaScriptFeature(
    BrowserState* browser_state)
    : JavaScriptFeature(
          // This feature operates in the page content world because the
          // script message is sent from restore_session.html which is loaded
          // into the webview.
          ContentWorld::kPageContentWorld,
          {}),
      browser_state_(browser_state),
      weak_factory_(this) {}

SessionRestoreJavaScriptFeature::~SessionRestoreJavaScriptFeature() = default;

void SessionRestoreJavaScriptFeature::ConfigureHandlers(
    WKUserContentController* user_content_controller) {
  // Reset the old handler first as handlers with the same name can not be
  // added simultaneously.
  session_restore_handler_.reset();

  session_restore_handler_ = std::make_unique<ScopedWKScriptMessageHandler>(
      user_content_controller, kSessionRestoreScriptHandlerName,
      base::BindRepeating(
          &SessionRestoreJavaScriptFeature::SessionRestorationMessageReceived,
          weak_factory_.GetWeakPtr()));
}

void SessionRestoreJavaScriptFeature::SessionRestorationMessageReceived(
    WKScriptMessage* message) {
  WebState* web_state = WebViewWebStateMap::FromBrowserState(browser_state_)
                            ->GetWebStateForWebView(message.webView);
  if (!web_state ||
      !web_state->GetNavigationManager()->IsRestoreSessionInProgress()) {
    // Ignore this message if `message.webView` is no longer associated with a
    // WebState or if session restore is not in progress.
    return;
  }

  if (![message.body[@"offset"] isKindOfClass:[NSNumber class]]) {
    return;
  }
  NSString* method =
      [NSString stringWithFormat:@"_crFinishSessionRestoration('%@')",
                                 message.body[@"offset"]];

  // Don't use `CallJavaScriptFunction` here, as it relies on the WebFrame
  // existing before window.onload starts.
  // Note that `web::ExecuteJavaScript` assumes the page content world, which is
  // ok in this case as restore_session.html is loaded as a webpage.
  web::ExecuteJavaScript(message.webView, method, nil);
}

}  // namespace web
