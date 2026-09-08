// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_PASSWORD_PROTECTION_JAVA_SCRIPT_FEATURE_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_PASSWORD_PROTECTION_JAVA_SCRIPT_FEATURE_H_

#include <map>
#include <memory>

#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ios/web/public/js_messaging/java_script_feature.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

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

  // This feature is a singleton that manages per-WebState state for
  // observers and rate limiting.
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

  // Maps WebStates to the timestamp of the last allowed paste event.
  absl::flat_hash_map<web::WebState*, base::TimeTicks> last_paste_timestamps_;

  // Maps WebStates to their pending paste key detection timers.
  absl::flat_hash_map<web::WebState*, std::unique_ptr<base::OneShotTimer>>
      paste_key_timers_;

  // Returns true if a paste event (shortcut or actual paste) for `web_state`
  // should be ignored due to rate limiting. Otherwise, updates the last paste
  // timestamp and returns false.
  bool IsPasteRateLimited(web::WebState* web_state);

  // Timer helper methods.
  void StartPasteKeyTimer(web::WebState* web_state);
  void OnPasteKeyTimerExpired(web::WebState* web_state);
};

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_PASSWORD_PROTECTION_JAVA_SCRIPT_FEATURE_H_
