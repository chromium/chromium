// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_PASSWORD_PROTECTION_JAVA_SCRIPT_FEATURE_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_PASSWORD_PROTECTION_JAVA_SCRIPT_FEATURE_H_

#include <map>

#include "ios/web/public/js_messaging/java_script_feature.h"

class InputEventObserver;

namespace web {
class WebState;
}

// A JavaScriptFeature that detects key presses and paste actions in the web
// content area.
class PasswordProtectionJavaScriptFeature : public web::JavaScriptFeature {
 public:
  PasswordProtectionJavaScriptFeature();
  ~PasswordProtectionJavaScriptFeature() override;

  // This feature holds no state, so only a single static instance is ever
  // needed.
  static PasswordProtectionJavaScriptFeature* GetInstance();

  // JavaScriptFeature:
  std::optional<std::string> GetScriptMessageHandlerName() const override;
  void ScriptMessageReceived(web::WebState* web_state,
                             const web::ScriptMessage& message) override;

  // Adds observer for key presses and paste actions, only for the WebState
  // specified in `observer`. It is an error to add more than one observer per
  // WebState, or more than one WebState per observer.
  void AddObserver(InputEventObserver* observer);

  // Removes the observer. It is an error to call this method if `observer` is
  // not already added.
  void RemoveObserver(InputEventObserver* observer);

 private:
  // Maps of WebStates and observers. An ObserverList is not needed since only
  // one observer is notified per event.
  std::map<web::WebState*, InputEventObserver*> lookup_by_web_state_;
  std::map<InputEventObserver*, web::WebState*> lookup_by_observer_;
};

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_PASSWORD_PROTECTION_JAVA_SCRIPT_FEATURE_H_
