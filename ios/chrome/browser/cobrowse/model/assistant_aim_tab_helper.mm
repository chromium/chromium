// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/model/assistant_aim_tab_helper.h"

#import "ios/chrome/browser/cobrowse/model/cobrowse_util.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "third_party/lens_server_proto/aim_communication.pb.h"

AssistantAimTabHelper::AssistantAimTabHelper(
    web::WebState* web_state,
    AimEligibilityService* aim_eligibility_service,
    PrefService* prefs)
    : aim_eligibility_service_(aim_eligibility_service), prefs_(prefs) {
  observation_.Observe(web_state);
}

AssistantAimTabHelper::~AssistantAimTabHelper() = default;

bool AssistantAimTabHelper::IsEligible() const {
  return IsAimCobrowseEligible(aim_eligibility_service_, prefs_);
}

void AssistantAimTabHelper::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (navigation_context->IsSameDocument()) {
    return;
  }
  handshake_received_ = false;
}

void AssistantAimTabHelper::WebStateDestroyed(web::WebState* web_state) {
  observation_.Reset();
  aim_eligibility_service_ = nullptr;
  prefs_ = nullptr;
}

void AssistantAimTabHelper::SetMessageCallback(MessageCallback callback) {
  message_callback_ = std::move(callback);
}

void AssistantAimTabHelper::OnMessageReceived(
    const lens::AimToClientMessage& message) {
  if (message.has_handshake_response()) {
    if (handshake_received_) {
      // Handshake was already processed.
      return;
    }
    handshake_received_ = true;
  }

  if (message_callback_) {
    message_callback_.Run(message);
  }
}
