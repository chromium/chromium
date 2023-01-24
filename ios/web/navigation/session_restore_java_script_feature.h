// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_SESSION_RESTORE_JAVA_SCRIPT_FEATURE_H_
#define IOS_WEB_NAVIGATION_SESSION_RESTORE_JAVA_SCRIPT_FEATURE_H_

#import <WebKit/WebKit.h>

#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#import "ios/web/js_messaging/scoped_wk_script_message_handler.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

namespace web {
class BrowserState;

// The session restoration script needs to use IPC to notify the app of
// the last step of the session restoration (which sets the page offset).
// See the restore_session.html file or crbug.com/1127521.
class SessionRestoreJavaScriptFeature : public base::SupportsUserData::Data,
                                        public JavaScriptFeature {
 public:
  // Returns the SessionRestoreJavaScriptFeature associated with
  // `browser_state`, creating one if necessary. `browser_state` must not be
  // null.
  static SessionRestoreJavaScriptFeature* FromBrowserState(
      BrowserState* browser_state);
  SessionRestoreJavaScriptFeature(BrowserState* browser_state);
  ~SessionRestoreJavaScriptFeature() override;

  void ConfigureHandlers(WKUserContentController* user_content_controller);

 private:
  SessionRestoreJavaScriptFeature(const SessionRestoreJavaScriptFeature&) =
      delete;
  SessionRestoreJavaScriptFeature& operator=(
      const SessionRestoreJavaScriptFeature&) = delete;

  // Handles a message from JavaScript to complete session restoration.
  void SessionRestorationMessageReceived(WKScriptMessage* script_message);

  // The browser state associated with this instance of the feature.
  BrowserState* browser_state_;

  // This feature uses ScopedWKScriptMessageHandler directly instead of the
  // message handling built into JavaScriptFeature because the WKWebView is used
  // to message the WKWebView directly since WebFrames are not yet setup during
  // session restoration. (The WKWebView is intentionally hidden from
  // JavaScriptFeature::ScriptMessageReceived).
  std::unique_ptr<ScopedWKScriptMessageHandler> session_restore_handler_;

  base::WeakPtrFactory<SessionRestoreJavaScriptFeature> weak_factory_;
};

}  // namespace web

#endif  // IOS_WEB_NAVIGATION_SESSION_RESTORE_JAVA_SCRIPT_FEATURE_H_
