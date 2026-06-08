// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/debugger/aim_srp_debugger_event.h"

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/lens_server_proto/aim_communication.pb.h"

// Test description: Verifies that AimSRPDebuggerEvent correctly parses and maps
// incoming and outgoing AIM communications to correct message names.
using AimSRPDebuggerEventTest = PlatformTest;

// Verifies parsing of HandshakePing from ClientToAimMessage.
TEST_F(AimSRPDebuggerEventTest, ParseHandshakePing) {
  lens::ClientToAimMessage message;
  auto* ping = message.mutable_handshake_ping();
  ping->add_capabilities(lens::FeatureCapability::DEFAULT);

  AimSRPDebuggerEvent* event =
      [[AimSRPDebuggerEvent alloc] initWithDirection:kClientToAim
                                  clientToAimMessage:message];

  EXPECT_EQ(event.direction, kClientToAim);
  EXPECT_NSEQ(event.messageName, @"HandshakePing");
}

// Verifies parsing of SubmitQuery from ClientToAimMessage.
TEST_F(AimSRPDebuggerEventTest, ParseSubmitQuery) {
  lens::ClientToAimMessage message;
  message.mutable_submit_query();

  AimSRPDebuggerEvent* event =
      [[AimSRPDebuggerEvent alloc] initWithDirection:kClientToAim
                                  clientToAimMessage:message];

  EXPECT_EQ(event.direction, kClientToAim);
  EXPECT_NSEQ(event.messageName, @"SubmitQuery");
}

// Verifies parsing of OpenThreadsView from ClientToAimMessage.
TEST_F(AimSRPDebuggerEventTest, ParseOpenThreadsView) {
  lens::ClientToAimMessage message;
  message.mutable_open_threads_view();

  AimSRPDebuggerEvent* event =
      [[AimSRPDebuggerEvent alloc] initWithDirection:kClientToAim
                                  clientToAimMessage:message];

  EXPECT_NSEQ(event.messageName, @"OpenThreadsView");
}

// Verifies parsing of SetCobrowsingDisplayMode from ClientToAimMessage.
TEST_F(AimSRPDebuggerEventTest, ParseSetCobrowsingDisplayMode) {
  lens::ClientToAimMessage message;
  message.mutable_set_cobrowsing_display_mode();

  AimSRPDebuggerEvent* event =
      [[AimSRPDebuggerEvent alloc] initWithDirection:kClientToAim
                                  clientToAimMessage:message];

  EXPECT_NSEQ(event.messageName, @"SetCobrowsingDisplayMode");
}

// Verifies parsing of InjectedInputUpdate from ClientToAimMessage.
TEST_F(AimSRPDebuggerEventTest, ParseInjectedInputUpdate) {
  lens::ClientToAimMessage message;
  message.mutable_injected_input_update();

  AimSRPDebuggerEvent* event =
      [[AimSRPDebuggerEvent alloc] initWithDirection:kClientToAim
                                  clientToAimMessage:message];

  EXPECT_NSEQ(event.messageName, @"InjectedInputUpdate");
}

// Verifies parsing of an empty/unknown ClientToAimMessage.
TEST_F(AimSRPDebuggerEventTest, ParseUnknownClientToAimMessage) {
  lens::ClientToAimMessage message;

  AimSRPDebuggerEvent* event =
      [[AimSRPDebuggerEvent alloc] initWithDirection:kClientToAim
                                  clientToAimMessage:message];

  EXPECT_NSEQ(event.messageName, @"Unknown ClientToAimMessage");
}

// Verifies parsing of HandshakeResponse from AimToClientMessage.
TEST_F(AimSRPDebuggerEventTest, ParseHandshakeResponse) {
  lens::AimToClientMessage message;
  message.mutable_handshake_response();

  AimSRPDebuggerEvent* event =
      [[AimSRPDebuggerEvent alloc] initWithDirection:kAimToClient
                                  aimToClientMessage:message];

  EXPECT_EQ(event.direction, kAimToClient);
  EXPECT_NSEQ(event.messageName, @"HandshakeResponse");
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
  message.mutable_update_thread_context_library();

  AimSRPDebuggerEvent* event =
      [[AimSRPDebuggerEvent alloc] initWithDirection:kAimToClient
                                  aimToClientMessage:message];

  EXPECT_NSEQ(event.messageName, @"UpdateThreadContextLibrary");
}

// Verifies parsing of NotifyZeroStateRendered from AimToClientMessage.
TEST_F(AimSRPDebuggerEventTest, ParseNotifyZeroStateRendered) {
  lens::AimToClientMessage message;
  message.mutable_notify_zero_state_rendered();

  AimSRPDebuggerEvent* event =
      [[AimSRPDebuggerEvent alloc] initWithDirection:kAimToClient
                                  aimToClientMessage:message];

  EXPECT_NSEQ(event.messageName, @"NotifyZeroStateRendered");
}

// Verifies parsing of SetChromeDesktopInputPlateConfiguration from
// AimToClientMessage.
TEST_F(AimSRPDebuggerEventTest, ParseSetChromeDesktopInputPlateConfiguration) {
  lens::AimToClientMessage message;
  message.mutable_set_chrome_desktop_input_plate_configuration();

  AimSRPDebuggerEvent* event =
      [[AimSRPDebuggerEvent alloc] initWithDirection:kAimToClient
                                  aimToClientMessage:message];

  EXPECT_NSEQ(event.messageName, @"SetChromeDesktopInputPlateConfiguration");
}

// Verifies parsing of InjectInput from AimToClientMessage.
TEST_F(AimSRPDebuggerEventTest, ParseInjectInput) {
  lens::AimToClientMessage message;
  message.mutable_inject_input();

  AimSRPDebuggerEvent* event =
      [[AimSRPDebuggerEvent alloc] initWithDirection:kAimToClient
                                  aimToClientMessage:message];

  EXPECT_NSEQ(event.messageName, @"InjectInput");
}

// Verifies parsing of RemoveInjectedInput from AimToClientMessage.
TEST_F(AimSRPDebuggerEventTest, ParseRemoveInjectedInput) {
  lens::AimToClientMessage message;
  message.mutable_remove_injected_input();

  AimSRPDebuggerEvent* event =
      [[AimSRPDebuggerEvent alloc] initWithDirection:kAimToClient
                                  aimToClientMessage:message];

  EXPECT_NSEQ(event.messageName, @"RemoveInjectedInput");
}

// Verifies parsing of OpenLinkInSidePanelMode from AimToClientMessage.
TEST_F(AimSRPDebuggerEventTest, ParseOpenLinkInSidePanelMode) {
  lens::AimToClientMessage message;
  message.mutable_open_link_in_side_panel_mode();

  AimSRPDebuggerEvent* event =
      [[AimSRPDebuggerEvent alloc] initWithDirection:kAimToClient
                                  aimToClientMessage:message];

  EXPECT_NSEQ(event.messageName, @"OpenLinkInSidePanelMode");
}

// Verifies parsing of an empty/unknown AimToClientMessage.
TEST_F(AimSRPDebuggerEventTest, ParseUnknownAimToClientMessage) {
  lens::AimToClientMessage message;

  AimSRPDebuggerEvent* event =
      [[AimSRPDebuggerEvent alloc] initWithDirection:kAimToClient
                                  aimToClientMessage:message];

  EXPECT_NSEQ(event.messageName, @"Unknown AimToClientMessage");
}
