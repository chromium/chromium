// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/model/assistant_aim_tab_helper.h"

#import "ios/web/public/navigation/navigation_context.h"
#import "third_party/lens_server_proto/aim_communication.pb.h"

AssistantAimTabHelper::AssistantAimTabHelper(web::WebState* web_state) {
  observation_.Observe(web_state);
}

AssistantAimTabHelper::~AssistantAimTabHelper() = default;

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
