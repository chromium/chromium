// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/incognito/incognito_lock_view_controller.h"

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_constants.h"
#import "ios/chrome/browser/ui/settings/privacy/incognito/incognito_lock_mutator.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_protocol.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "third_party/ocmock/ocmock_extensions.h"

namespace {

// Index of the respective items on the Incognito lock settings screen.
NSIndexPath* kReauthIndexPath = [NSIndexPath indexPathForRow:0 inSection:0];
NSIndexPath* kSoftLockIndexPath = [NSIndexPath indexPathForRow:1 inSection:0];
NSIndexPath* kNoneIndexPath = [NSIndexPath indexPathForRow:2 inSection:0];

// Matcher for the reauthentication handler callback. Besides matching the
// handler, it also sets the reauthentication result.
auto ReauthCompletionHandler(ReauthenticationResult result) {
  return [OCMArg
      checkWithBlock:^BOOL(void (^completion_handler)(ReauthenticationResult)) {
        completion_handler(/*result=*/result);
        return YES;
      }];
}
}  // namespace

// Test class for the IncognitoLockViewController.
class IncognitoLockViewControllerTest : public PlatformTest {
 protected:
  IncognitoLockViewControllerTest() {
    mutator_ = [OCMockObject mockForProtocol:@protocol(IncognitoLockMutator)];
    reauth_module_ =
        [OCMockObject mockForProtocol:@protocol(ReauthenticationProtocol)];
    OCMStub([reauth_module_ canAttemptReauth]).andReturn(YES);
  }

  void SetUp() override {
    view_controller_ = [[IncognitoLockViewController alloc]
        initWithReauthModule:reauth_module_];
    view_controller_.mutator = mutator_;
  }

  IncognitoLockViewController* view_controller_;
  id mutator_;
  id reauth_module_;
};

// Test that a click on the None setting, when already on the None state, does
// nothing.
TEST_F(IncognitoLockViewControllerTest, NoneToNone) {
  [view_controller_ setIncognitoLockState:IncognitoLockState::kNone];

  OCMReject([reauth_module_ attemptReauthWithLocalizedReason:[OCMArg any]
                                        canReusePreviousAuth:NO
                                                     handler:[OCMArg any]]);
  OCMReject([mutator_ updateIncognitoLockState:IncognitoLockState::kNone]);

  [view_controller_ tableView:view_controller_.tableView
      didSelectRowAtIndexPath:kNoneIndexPath];

  EXPECT_OCMOCK_VERIFY(reauth_module_);
  EXPECT_OCMOCK_VERIFY(mutator_);
}

// Test that a transition from the None to the SoftLock state does not require
// authentication and correctly triggers the state change via the mutator.
TEST_F(IncognitoLockViewControllerTest, NoneToSoftLock) {
  [view_controller_ setIncognitoLockState:IncognitoLockState::kNone];

  OCMReject([reauth_module_ attemptReauthWithLocalizedReason:[OCMArg any]
                                        canReusePreviousAuth:NO
                                                     handler:[OCMArg any]]);
  OCMExpect([mutator_ updateIncognitoLockState:IncognitoLockState::kSoftLock]);

  [view_controller_ tableView:view_controller_.tableView
      didSelectRowAtIndexPath:kSoftLockIndexPath];

  EXPECT_OCMOCK_VERIFY(reauth_module_);
  EXPECT_OCMOCK_VERIFY(mutator_);
}

// Test that a transition from the None to the Reauth state, with a failed
// authentication attempt, does nothing.
TEST_F(IncognitoLockViewControllerTest, NoneToReauthWithFailure) {
  [view_controller_ setIncognitoLockState:IncognitoLockState::kNone];

  OCMExpect([reauth_module_
      attemptReauthWithLocalizedReason:[OCMArg any]
                  canReusePreviousAuth:NO
                               handler:ReauthCompletionHandler(
                                           ReauthenticationResult::kFailure)]);
  OCMReject([mutator_ updateIncognitoLockState:IncognitoLockState::kReauth]);

  [view_controller_ tableView:view_controller_.tableView
      didSelectRowAtIndexPath:kReauthIndexPath];

  EXPECT_OCMOCK_VERIFY(reauth_module_);
  EXPECT_OCMOCK_VERIFY(mutator_);
}

// Test that a transition from the None to the Reauth state, with a successful
// authentication attempt, correctly triggers the state change via the mutator.
TEST_F(IncognitoLockViewControllerTest, NoneToReauthWithSuccess) {
  [view_controller_ setIncognitoLockState:IncognitoLockState::kNone];

  OCMExpect([reauth_module_
      attemptReauthWithLocalizedReason:[OCMArg any]
                  canReusePreviousAuth:NO
                               handler:ReauthCompletionHandler(
                                           ReauthenticationResult::kSuccess)]);
  OCMExpect([mutator_ updateIncognitoLockState:IncognitoLockState::kReauth]);

  [view_controller_ tableView:view_controller_.tableView
      didSelectRowAtIndexPath:kReauthIndexPath];

  EXPECT_OCMOCK_VERIFY(reauth_module_);
  EXPECT_OCMOCK_VERIFY(mutator_);
}

// Test that a click on the SoftLock setting, when already on the SoftLock
// state, does nothing.
TEST_F(IncognitoLockViewControllerTest, SoftLockToSoftLock) {
  [view_controller_ setIncognitoLockState:IncognitoLockState::kSoftLock];

  OCMReject([reauth_module_ attemptReauthWithLocalizedReason:[OCMArg any]
                                        canReusePreviousAuth:NO
                                                     handler:[OCMArg any]]);
  OCMReject([mutator_ updateIncognitoLockState:IncognitoLockState::kSoftLock]);

  [view_controller_ tableView:view_controller_.tableView
      didSelectRowAtIndexPath:kSoftLockIndexPath];

  EXPECT_OCMOCK_VERIFY(reauth_module_);
  EXPECT_OCMOCK_VERIFY(mutator_);
}

// Test that a transition from the SoftLock to the None state does not require
// authentication and correctly triggers the state change via the mutator.
TEST_F(IncognitoLockViewControllerTest, SoftLockToNone) {
  [view_controller_ setIncognitoLockState:IncognitoLockState::kSoftLock];

  OCMReject([reauth_module_ attemptReauthWithLocalizedReason:[OCMArg any]
                                        canReusePreviousAuth:NO
                                                     handler:[OCMArg any]]);
  OCMExpect([mutator_ updateIncognitoLockState:IncognitoLockState::kNone]);

  [view_controller_ tableView:view_controller_.tableView
      didSelectRowAtIndexPath:kNoneIndexPath];

  EXPECT_OCMOCK_VERIFY(reauth_module_);
  EXPECT_OCMOCK_VERIFY(mutator_);
}

// Test that a transition from the SoftLock to the Reauth state, with a failed
// authentication attempt, does nothing.
TEST_F(IncognitoLockViewControllerTest, SoftLockToReauthWithFailure) {
  [view_controller_ setIncognitoLockState:IncognitoLockState::kSoftLock];

  OCMExpect([reauth_module_
      attemptReauthWithLocalizedReason:[OCMArg any]
                  canReusePreviousAuth:NO
                               handler:ReauthCompletionHandler(
                                           ReauthenticationResult::kFailure)]);
  OCMReject([mutator_ updateIncognitoLockState:IncognitoLockState::kReauth]);

  [view_controller_ tableView:view_controller_.tableView
      didSelectRowAtIndexPath:kReauthIndexPath];

  EXPECT_OCMOCK_VERIFY(reauth_module_);
  EXPECT_OCMOCK_VERIFY(mutator_);
}

// Test that a transition from the SoftLock to the Reauth state, with a
// successful authentication attempt, correctly triggers the state change via
// the mutator.
TEST_F(IncognitoLockViewControllerTest, SoftLockToReauthWithSuccess) {
  [view_controller_ setIncognitoLockState:IncognitoLockState::kSoftLock];

  OCMExpect([reauth_module_
      attemptReauthWithLocalizedReason:[OCMArg any]
                  canReusePreviousAuth:NO
                               handler:ReauthCompletionHandler(
                                           ReauthenticationResult::kSuccess)]);
  OCMExpect([mutator_ updateIncognitoLockState:IncognitoLockState::kReauth]);

  [view_controller_ tableView:view_controller_.tableView
      didSelectRowAtIndexPath:kReauthIndexPath];

  EXPECT_OCMOCK_VERIFY(reauth_module_);
  EXPECT_OCMOCK_VERIFY(mutator_);
}

// Test that a click on the Reauth setting, when already on the Reauth state,
// does nothing.
TEST_F(IncognitoLockViewControllerTest, ReauthToReauth) {
  [view_controller_ setIncognitoLockState:IncognitoLockState::kReauth];

  OCMReject([reauth_module_ attemptReauthWithLocalizedReason:[OCMArg any]
                                        canReusePreviousAuth:NO
                                                     handler:[OCMArg any]]);
  OCMReject([mutator_ updateIncognitoLockState:IncognitoLockState::kReauth]);

  [view_controller_ tableView:view_controller_.tableView
      didSelectRowAtIndexPath:kReauthIndexPath];

  EXPECT_OCMOCK_VERIFY(reauth_module_);
  EXPECT_OCMOCK_VERIFY(mutator_);
}

// Test that a transition from the Reauth to the None state, with a failed
// authentication attempt, does nothing.
TEST_F(IncognitoLockViewControllerTest, ReauthToNoneWithFailure) {
  [view_controller_ setIncognitoLockState:IncognitoLockState::kReauth];

  OCMExpect([reauth_module_
      attemptReauthWithLocalizedReason:[OCMArg any]
                  canReusePreviousAuth:NO
                               handler:ReauthCompletionHandler(
                                           ReauthenticationResult::kFailure)]);
  OCMReject([mutator_ updateIncognitoLockState:IncognitoLockState::kNone]);

  [view_controller_ tableView:view_controller_.tableView
      didSelectRowAtIndexPath:kNoneIndexPath];

  EXPECT_OCMOCK_VERIFY(reauth_module_);
  EXPECT_OCMOCK_VERIFY(mutator_);
}

// Test that a transition from the Reauth to the None state, with a
// successful authentication attempt, correctly triggers the state change via
// the mutator.
TEST_F(IncognitoLockViewControllerTest, ReauthToNoneWithSuccess) {
  [view_controller_ setIncognitoLockState:IncognitoLockState::kReauth];

  OCMExpect([reauth_module_
      attemptReauthWithLocalizedReason:[OCMArg any]
                  canReusePreviousAuth:NO
                               handler:ReauthCompletionHandler(
                                           ReauthenticationResult::kSuccess)]);
  OCMExpect([mutator_ updateIncognitoLockState:IncognitoLockState::kNone]);

  [view_controller_ tableView:view_controller_.tableView
      didSelectRowAtIndexPath:kNoneIndexPath];

  EXPECT_OCMOCK_VERIFY(reauth_module_);
  EXPECT_OCMOCK_VERIFY(mutator_);
}

// Test that a transition from the Reauth to the SoftLock state, with a failed
// authentication attempt, does nothing.
TEST_F(IncognitoLockViewControllerTest, ReauthToSoftLockWithFailure) {
  [view_controller_ setIncognitoLockState:IncognitoLockState::kReauth];

  OCMExpect([reauth_module_
      attemptReauthWithLocalizedReason:[OCMArg any]
                  canReusePreviousAuth:NO
                               handler:ReauthCompletionHandler(
                                           ReauthenticationResult::kFailure)]);
  OCMReject([mutator_ updateIncognitoLockState:IncognitoLockState::kSoftLock]);

  [view_controller_ tableView:view_controller_.tableView
      didSelectRowAtIndexPath:kSoftLockIndexPath];

  EXPECT_OCMOCK_VERIFY(reauth_module_);
  EXPECT_OCMOCK_VERIFY(mutator_);
}

// Test that a transition from the Reauth to the SoftLock state, with a
// successful authentication attempt, correctly triggers the state change via
// the mutator.
TEST_F(IncognitoLockViewControllerTest, ReauthToSoftLockWithSuccess) {
  [view_controller_ setIncognitoLockState:IncognitoLockState::kReauth];

  OCMExpect([reauth_module_
      attemptReauthWithLocalizedReason:[OCMArg any]
                  canReusePreviousAuth:NO
                               handler:ReauthCompletionHandler(
                                           ReauthenticationResult::kSuccess)]);
  OCMExpect([mutator_ updateIncognitoLockState:IncognitoLockState::kSoftLock]);

  [view_controller_ tableView:view_controller_.tableView
      didSelectRowAtIndexPath:kSoftLockIndexPath];

  EXPECT_OCMOCK_VERIFY(reauth_module_);
  EXPECT_OCMOCK_VERIFY(mutator_);
}
