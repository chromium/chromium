// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/debugger/aim_srp_message_logger.h"

#import "ios/chrome/browser/cobrowse/debugger/aim_srp_debugger_event.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/lens_server_proto/aim_communication.pb.h"

// Test description: Verifies that AimSRPMessageLogger correctly stores, logs,
// and clears events for native and web communications.
class AimSRPMessageLoggerTest : public PlatformTest {
 protected:
  AimSRPMessageLoggerTest() { logger_ = [[AimSRPMessageLogger alloc] init]; }

  AimSRPMessageLogger* logger_;
};

// Verifies that AimSRPMessageLogger correctly stores logged events and handles
// clears.
TEST_F(AimSRPMessageLoggerTest, LoggerStoresAndClearsEvents) {
  EXPECT_EQ(logger_.events.count, 0u);

  lens::ClientToAimMessage client_msg;
  client_msg.mutable_handshake_ping();
  [logger_ logClientToAimMessage:client_msg];

  EXPECT_EQ(logger_.events.count, 1u);
  EXPECT_NSEQ(logger_.events[0].messageName, @"HandshakePing");

  lens::AimToClientMessage srp_msg;
  srp_msg.mutable_hide_input();
  [logger_ logAimToClientMessage:srp_msg];

  EXPECT_EQ(logger_.events.count, 2u);
  EXPECT_NSEQ(logger_.events[1].messageName, @"HideInput");

  [logger_ clearEvents];
  EXPECT_EQ(logger_.events.count, 0u);
}
