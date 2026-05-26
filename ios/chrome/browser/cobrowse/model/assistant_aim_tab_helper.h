// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COBROWSE_MODEL_ASSISTANT_AIM_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_COBROWSE_MODEL_ASSISTANT_AIM_TAB_HELPER_H_

#include "base/functional/callback.h"
#include "base/scoped_observation.h"
#include "ios/web/public/web_state_observer.h"
#include "ios/web/public/web_state_user_data.h"

namespace web {
class NavigationContext;
}  // namespace web

namespace lens {
class AimToClientMessage;
}  // namespace lens

// A tab helper that manages the web-to-native handshake and handles script
// messages forwarded by the AimCobrowseJavaScriptFeature.
class AssistantAimTabHelper
    : public web::WebStateObserver,
      public web::WebStateUserData<AssistantAimTabHelper> {
 public:
  using MessageCallback =
      base::RepeatingCallback<void(const lens::AimToClientMessage&)>;

  AssistantAimTabHelper(const AssistantAimTabHelper&) = delete;
  AssistantAimTabHelper& operator=(const AssistantAimTabHelper&) = delete;

  ~AssistantAimTabHelper() override;

  // Sets the callback to be invoked when an AimToClientMessage is received.
  void SetMessageCallback(MessageCallback callback);

  // Returns whether the handshake response has been received.
  bool IsHandshakeReceived() const { return handshake_received_; }

  // Handles parsed messages forwarded by AimCobrowseJavaScriptFeature.
  void OnMessageReceived(const lens::AimToClientMessage& message);

  // web::WebStateObserver:
  void DidStartNavigation(web::WebState* web_state,
                          web::NavigationContext* navigation_context) override;
  void WebStateDestroyed(web::WebState* web_state) override;

 private:
  friend class web::WebStateUserData<AssistantAimTabHelper>;
  explicit AssistantAimTabHelper(web::WebState* web_state);

  bool handshake_received_ = false;
  MessageCallback message_callback_;

  base::ScopedObservation<web::WebState, web::WebStateObserver> observation_{
      this};
};

#endif  // IOS_CHROME_BROWSER_COBROWSE_MODEL_ASSISTANT_AIM_TAB_HELPER_H_
