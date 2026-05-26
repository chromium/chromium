// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/model/assistant_aim_tab_helper.h"

#import "base/functional/callback.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/lens_server_proto/aim_communication.pb.h"

class AssistantAimTabHelperTest : public PlatformTest {
 protected:
  web::FakeWebState web_state_;
};

// Tests that receiving a handshake response sets IsHandshakeReceived to true.
TEST_F(AssistantAimTabHelperTest, HandshakeResponseReceived) {
  AssistantAimTabHelper::CreateForWebState(&web_state_);
  AssistantAimTabHelper* tab_helper =
      AssistantAimTabHelper::FromWebState(&web_state_);
  ASSERT_NE(tab_helper, nullptr);
  EXPECT_FALSE(tab_helper->IsHandshakeReceived());

  lens::AimToClientMessage handshake_response;
  handshake_response.mutable_handshake_response();

  tab_helper->OnMessageReceived(handshake_response);
  EXPECT_TRUE(tab_helper->IsHandshakeReceived());
}

// Tests that starting a navigation resets the handshake status.
TEST_F(AssistantAimTabHelperTest, NavigationResetsHandshake) {
  AssistantAimTabHelper::CreateForWebState(&web_state_);
  AssistantAimTabHelper* tab_helper =
      AssistantAimTabHelper::FromWebState(&web_state_);
  ASSERT_NE(tab_helper, nullptr);

  // Set handshake to received.
  lens::AimToClientMessage handshake_response;
  handshake_response.mutable_handshake_response();
  tab_helper->OnMessageReceived(handshake_response);
  ASSERT_TRUE(tab_helper->IsHandshakeReceived());

  // Simulate navigation to a new document.
  web::FakeNavigationContext context;
  context.SetIsSameDocument(false);
  tab_helper->DidStartNavigation(&web_state_, &context);

  EXPECT_FALSE(tab_helper->IsHandshakeReceived());
}

// Tests that same document navigations do not reset the handshake status.
TEST_F(AssistantAimTabHelperTest, SameDocumentNavigationDoesNotResetHandshake) {
  AssistantAimTabHelper::CreateForWebState(&web_state_);
  AssistantAimTabHelper* tab_helper =
      AssistantAimTabHelper::FromWebState(&web_state_);
  ASSERT_NE(tab_helper, nullptr);

  // Set handshake to received.
  lens::AimToClientMessage handshake_response;
  handshake_response.mutable_handshake_response();
  tab_helper->OnMessageReceived(handshake_response);
  ASSERT_TRUE(tab_helper->IsHandshakeReceived());

  // Simulate same-document navigation.
  web::FakeNavigationContext context;
  context.SetIsSameDocument(true);
  tab_helper->DidStartNavigation(&web_state_, &context);

  EXPECT_TRUE(tab_helper->IsHandshakeReceived());
}

// Tests that the message callback is successfully invoked with the parsed
// message.
TEST_F(AssistantAimTabHelperTest, MessageCallbackInvoked) {
  AssistantAimTabHelper::CreateForWebState(&web_state_);
  AssistantAimTabHelper* tab_helper =
      AssistantAimTabHelper::FromWebState(&web_state_);
  ASSERT_NE(tab_helper, nullptr);

  bool callback_invoked = false;
  bool hide_input_received = false;
  tab_helper->SetMessageCallback(base::BindRepeating(
      [](bool* invoked, bool* hide_received,
         const lens::AimToClientMessage& msg) {
        *invoked = true;
        if (msg.has_hide_input()) {
          *hide_received = true;
        }
      },
      &callback_invoked, &hide_input_received));

  lens::AimToClientMessage aim_message;
  aim_message.mutable_hide_input();

  tab_helper->OnMessageReceived(aim_message);

  EXPECT_TRUE(callback_invoked);
  EXPECT_TRUE(hide_input_received);
}
