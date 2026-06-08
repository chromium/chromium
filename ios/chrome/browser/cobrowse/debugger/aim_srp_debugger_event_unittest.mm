// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/debugger/aim_srp_debugger_event.h"

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/lens_server_proto/aim_communication.pb.h"

// Test description: Verifies that AimSRPDebuggerEvent correctly parses all
// types of incoming and outgoing AIM communications.
using AimSRPDebuggerEventTest = PlatformTest;

// Verifies parsing of HandshakePing from ClientToAimMessage.
TEST_F(AimSRPDebuggerEventTest, ParseHandshakePing) {
  lens::ClientToAimMessage message;
  auto* ping = message.mutable_handshake_ping();
  ping->add_capabilities(lens::FeatureCapability::DEFAULT);
  ping->add_capabilities(lens::FeatureCapability::LOCK_INPUT);

  AimSRPDebuggerEvent* event =
      [[AimSRPDebuggerEvent alloc] initWithDirection:kClientToAim
                                  clientToAimMessage:message];

  EXPECT_EQ(event.direction, kClientToAim);
  EXPECT_NSEQ(event.messageName, @"HandshakePing");
  EXPECT_TRUE([event.messagePayload containsString:@"Capabilities:"]);
  EXPECT_TRUE([event.messagePayload containsString:@"• DEFAULT"]);
  EXPECT_TRUE([event.messagePayload containsString:@"• LOCK_INPUT"]);
}

// Verifies parsing of SubmitQuery from ClientToAimMessage.
TEST_F(AimSRPDebuggerEventTest, ParseSubmitQuery) {
  lens::ClientToAimMessage message;
  auto* submit_query = message.mutable_submit_query();
  auto* payload = submit_query->mutable_payload();
  payload->set_query_text("test query");
  payload->mutable_added_inputs()->add_added_inputs();

  AimSRPDebuggerEvent* event =
      [[AimSRPDebuggerEvent alloc] initWithDirection:kClientToAim
                                  clientToAimMessage:message];

  EXPECT_EQ(event.direction, kClientToAim);
  EXPECT_NSEQ(event.messageName, @"SubmitQuery");
  EXPECT_TRUE([event.messagePayload containsString:@"Query Text: test query"]);
  EXPECT_TRUE([event.messagePayload containsString:@"Added inputs count: 1"]);
}

// Verifies parsing of OpenThreadsView from ClientToAimMessage.
TEST_F(AimSRPDebuggerEventTest, ParseOpenThreadsView) {
  lens::ClientToAimMessage message;
  message.mutable_open_threads_view();

  AimSRPDebuggerEvent* event =
      [[AimSRPDebuggerEvent alloc] initWithDirection:kClientToAim
                                  clientToAimMessage:message];

  EXPECT_NSEQ(event.messageName, @"OpenThreadsView");
  EXPECT_EQ(event.messagePayload, nil);
}

// Verifies parsing of SetCobrowsingDisplayMode from ClientToAimMessage.
TEST_F(AimSRPDebuggerEventTest, ParseSetCobrowsingDisplayMode) {
  lens::ClientToAimMessage message;
  message.mutable_set_cobrowsing_display_mode()
      ->mutable_params()
      ->set_display_mode(
          lens::CobrowsingDisplayModeParams_DisplayMode_COBROWSING_SIDEPANEL);

  AimSRPDebuggerEvent* event =
      [[AimSRPDebuggerEvent alloc] initWithDirection:kClientToAim
                                  clientToAimMessage:message];

  EXPECT_NSEQ(event.messageName, @"SetCobrowsingDisplayMode");
  EXPECT_TRUE([event.messagePayload containsString:@"Display Mode: 2"]);
}

// Verifies parsing of InjectedInputUpdate from ClientToAimMessage.
TEST_F(AimSRPDebuggerEventTest, ParseInjectedInputUpdate) {
  lens::ClientToAimMessage message;
  auto* update = message.mutable_injected_input_update()->mutable_payload();
  update->set_id("element-123");
  update->set_update_type(lens::InjectedInputUpdatePayload_UpdateType_REMOVED);

  AimSRPDebuggerEvent* event =
      [[AimSRPDebuggerEvent alloc] initWithDirection:kClientToAim
                                  clientToAimMessage:message];

  EXPECT_NSEQ(event.messageName, @"InjectedInputUpdate");
  EXPECT_TRUE([event.messagePayload containsString:@"ID: element-123"]);
  EXPECT_TRUE([event.messagePayload containsString:@"Type: 1"]);
}

// Verifies parsing of an empty/unknown ClientToAimMessage.
TEST_F(AimSRPDebuggerEventTest, ParseUnknownClientToAimMessage) {
  lens::ClientToAimMessage message;

  AimSRPDebuggerEvent* event =
      [[AimSRPDebuggerEvent alloc] initWithDirection:kClientToAim
                                  clientToAimMessage:message];

  EXPECT_NSEQ(event.messageName, @"Unknown ClientToAimMessage");
  EXPECT_EQ(event.messagePayload, nil);
}

// Verifies parsing of HandshakeResponse from AimToClientMessage.
TEST_F(AimSRPDebuggerEventTest, ParseHandshakeResponse) {
  lens::AimToClientMessage message;
  auto* response = message.mutable_handshake_response();
  response->add_capabilities(lens::FeatureCapability::UNLOCK_INPUT);

  AimSRPDebuggerEvent* event =
      [[AimSRPDebuggerEvent alloc] initWithDirection:kAimToClient
                                  aimToClientMessage:message];

  EXPECT_EQ(event.direction, kAimToClient);
  EXPECT_NSEQ(event.messageName, @"HandshakeResponse");
  EXPECT_TRUE([event.messagePayload containsString:@"Capabilities:"]);
  EXPECT_TRUE([event.messagePayload containsString:@"• UNLOCK_INPUT"]);
}

// Verifies parsing of simple AimToClientMessage actions.
TEST_F(AimSRPDebuggerEventTest, ParseSimpleAimToClientMessages) {
  // HideInput
  {
    lens::AimToClientMessage message;
    message.mutable_hide_input();
    AimSRPDebuggerEvent* event =
        [[AimSRPDebuggerEvent alloc] initWithDirection:kAimToClient
                                    aimToClientMessage:message];
    EXPECT_NSEQ(event.messageName, @"HideInput");
  }
  // RestoreInput
  {
    lens::AimToClientMessage message;
    message.mutable_restore_input();
    AimSRPDebuggerEvent* event =
        [[AimSRPDebuggerEvent alloc] initWithDirection:kAimToClient
                                    aimToClientMessage:message];
    EXPECT_NSEQ(event.messageName, @"RestoreInput");
  }
  // EnterBasicMode
  {
    lens::AimToClientMessage message;
    message.mutable_enter_basic_mode();
    AimSRPDebuggerEvent* event =
        [[AimSRPDebuggerEvent alloc] initWithDirection:kAimToClient
                                    aimToClientMessage:message];
    EXPECT_NSEQ(event.messageName, @"EnterBasicMode");
  }
  // ExitBasicMode
  {
    lens::AimToClientMessage message;
    message.mutable_exit_basic_mode();
    AimSRPDebuggerEvent* event =
        [[AimSRPDebuggerEvent alloc] initWithDirection:kAimToClient
                                    aimToClientMessage:message];
    EXPECT_NSEQ(event.messageName, @"ExitBasicMode");
  }
  // LockInput
  {
    lens::AimToClientMessage message;
    message.mutable_lock_input();
    AimSRPDebuggerEvent* event =
        [[AimSRPDebuggerEvent alloc] initWithDirection:kAimToClient
                                    aimToClientMessage:message];
    EXPECT_NSEQ(event.messageName, @"LockInput");
  }
  // UnlockInput
  {
    lens::AimToClientMessage message;
    message.mutable_unlock_input();
    AimSRPDebuggerEvent* event =
        [[AimSRPDebuggerEvent alloc] initWithDirection:kAimToClient
                                    aimToClientMessage:message];
    EXPECT_NSEQ(event.messageName, @"UnlockInput");
  }
}

// Verifies parsing of UpdateThreadContextLibrary from AimToClientMessage.
TEST_F(AimSRPDebuggerEventTest, ParseUpdateThreadContextLibrary) {
  lens::AimToClientMessage message;
  auto* library = message.mutable_update_thread_context_library();
  library->add_contexts();
  library->add_contexts();

  AimSRPDebuggerEvent* event =
      [[AimSRPDebuggerEvent alloc] initWithDirection:kAimToClient
                                  aimToClientMessage:message];

  EXPECT_NSEQ(event.messageName, @"UpdateThreadContextLibrary");
  EXPECT_TRUE([event.messagePayload containsString:@"Contexts size: 2"]);
}

// Verifies parsing of NotifyZeroStateRendered from AimToClientMessage.
TEST_F(AimSRPDebuggerEventTest, ParseNotifyZeroStateRendered) {
  // Test YES
  {
    lens::AimToClientMessage message;
    message.mutable_notify_zero_state_rendered()->set_is_zero_state_rendered(
        true);
    AimSRPDebuggerEvent* event =
        [[AimSRPDebuggerEvent alloc] initWithDirection:kAimToClient
                                    aimToClientMessage:message];
    EXPECT_NSEQ(event.messageName, @"NotifyZeroStateRendered");
    EXPECT_TRUE([event.messagePayload containsString:@"Rendered: YES"]);
  }
  // Test NO
  {
    lens::AimToClientMessage message;
    message.mutable_notify_zero_state_rendered()->set_is_zero_state_rendered(
        false);
    AimSRPDebuggerEvent* event =
        [[AimSRPDebuggerEvent alloc] initWithDirection:kAimToClient
                                    aimToClientMessage:message];
    EXPECT_NSEQ(event.messageName, @"NotifyZeroStateRendered");
    EXPECT_TRUE([event.messagePayload containsString:@"Rendered: NO"]);
  }
}

// Verifies parsing of SetChromeDesktopInputPlateConfiguration from
// AimToClientMessage.
TEST_F(AimSRPDebuggerEventTest, ParseSetChromeDesktopInputPlateConfiguration) {
  lens::AimToClientMessage message;
  auto* config = message.mutable_set_chrome_desktop_input_plate_configuration();
  config->set_max_width(400);
  config->set_max_height(600);
  config->set_margin_bottom(20);
  config->set_margin_left(15);

  AimSRPDebuggerEvent* event =
      [[AimSRPDebuggerEvent alloc] initWithDirection:kAimToClient
                                  aimToClientMessage:message];

  EXPECT_NSEQ(event.messageName, @"SetChromeDesktopInputPlateConfiguration");
  EXPECT_TRUE([event.messagePayload containsString:@"max_width: 400"]);
  EXPECT_TRUE([event.messagePayload containsString:@"max_height: 600"]);
  EXPECT_TRUE([event.messagePayload containsString:@"margin_bottom: 20"]);
  EXPECT_TRUE([event.messagePayload containsString:@"margin_left: 15"]);
}

// Verifies parsing of InjectInput from AimToClientMessage.
TEST_F(AimSRPDebuggerEventTest, ParseInjectInput) {
  lens::AimToClientMessage message;
  auto* inject = message.mutable_inject_input();
  inject->set_query_text("bike wheels");
  inject->set_expand(true);
  inject->set_submit_after_injection(false);

  AimSRPDebuggerEvent* event =
      [[AimSRPDebuggerEvent alloc] initWithDirection:kAimToClient
                                  aimToClientMessage:message];

  EXPECT_NSEQ(event.messageName, @"InjectInput");
  EXPECT_TRUE([event.messagePayload containsString:@"Query text: bike wheels"]);
  EXPECT_TRUE([event.messagePayload containsString:@"expand: YES"]);
  EXPECT_TRUE([event.messagePayload containsString:@"submit: NO"]);
}

// Verifies parsing of RemoveInjectedInput from AimToClientMessage.
TEST_F(AimSRPDebuggerEventTest, ParseRemoveInjectedInput) {
  lens::AimToClientMessage message;
  message.mutable_remove_injected_input()->set_id("input-555");

  AimSRPDebuggerEvent* event =
      [[AimSRPDebuggerEvent alloc] initWithDirection:kAimToClient
                                  aimToClientMessage:message];

  EXPECT_NSEQ(event.messageName, @"RemoveInjectedInput");
  EXPECT_TRUE([event.messagePayload containsString:@"ID: input-555"]);
}

// Verifies parsing of OpenLinkInSidePanelMode from AimToClientMessage.
TEST_F(AimSRPDebuggerEventTest, ParseOpenLinkInSidePanelMode) {
  lens::AimToClientMessage message;
  auto* link = message.mutable_open_link_in_side_panel_mode();
  link->set_url("https://google.com");

  AimSRPDebuggerEvent* event =
      [[AimSRPDebuggerEvent alloc] initWithDirection:kAimToClient
                                  aimToClientMessage:message];

  EXPECT_NSEQ(event.messageName, @"OpenLinkInSidePanelMode");
  EXPECT_NSEQ(event.messagePayload, @"URL: https://google.com");
}

// Verifies parsing of an empty/unknown AimToClientMessage.
TEST_F(AimSRPDebuggerEventTest, ParseUnknownAimToClientMessage) {
  lens::AimToClientMessage message;

  AimSRPDebuggerEvent* event =
      [[AimSRPDebuggerEvent alloc] initWithDirection:kAimToClient
                                  aimToClientMessage:message];

  EXPECT_NSEQ(event.messageName, @"Unknown AimToClientMessage");
  EXPECT_EQ(event.messagePayload, nil);
}
