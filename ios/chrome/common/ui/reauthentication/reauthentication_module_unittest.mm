// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <LocalAuthentication/LocalAuthentication.h>

#import "base/functional/callback_helpers.h"
#import "base/run_loop.h"
#import "base/test/simple_test_clock.h"
#import "base/test/task_environment.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module_for_testing.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

// Returns a ReauthenticationResultBlock that call QuitClosure on `run_loop`.
ReauthenticationResultBlock CallQuitClosure(base::RunLoop& run_loop) {
  return base::CallbackToBlock(
      base::IgnoreArgs<ReauthenticationResult>(run_loop.QuitClosure()));
}

class ReauthenticationModuleTest : public PlatformTest {
 protected:
  ReauthenticationModuleTest() {}

  void SetUp() override {
    auth_context_ = [OCMockObject niceMockForClass:[LAContext class]];
    reauth_module_ = [[ReauthenticationModule alloc] initWithClock:&clock_];
    [reauth_module_ setCreateLAContext:^LAContext*() {
      return auth_context_;
    }];
  }

  void TearDown() override {
    @autoreleasepool {
      reauth_module_ = nil;
      auth_context_ = nil;
    }
  }

  base::test::TaskEnvironment env_;
  base::SimpleTestClock clock_;
  id auth_context_;
  ReauthenticationModule* reauth_module_;
};

// Tests that reauthentication is not reused when reuse is not permitted
// even if the time interval since the previous reauthentication is less
// than 60 seconds.
TEST_F(ReauthenticationModuleTest, ReauthReuseNotPermitted) {
  // Pretends authentication was successful 20 seconds ago.
  [reauth_module_ setLastSuccessfulReauthTime:clock_.Now()];
  clock_.Advance(base::Seconds(20));

  OCMExpect([auth_context_ evaluatePolicy:LAPolicyDeviceOwnerAuthentication
                          localizedReason:[OCMArg any]
                                    reply:[OCMArg any]])
      .andDo(^(NSInvocation* invocation) {
        // Use -getArgument:atIndex: to access the block passed by the
        // implementation of ReauthenticationModule and invoke it. The
        // first two arguments are `self` and `cmd` so the index needs
        // to start at 2, thus 4 for the `reply` parameter.
        __unsafe_unretained void (^block)(BOOL, NSError*) = nil;
        [invocation getArgument:&block atIndex:4];
        block(YES, nil);
      });

  base::RunLoop run_loop;
  [reauth_module_ attemptReauthWithLocalizedReason:@"Test"
                              canReusePreviousAuth:NO
                                           handler:CallQuitClosure(run_loop)];

  run_loop.Run();

  EXPECT_OCMOCK_VERIFY(auth_context_);
}

// Tests that the previous reauthentication is reused when reuse is permitted
// and the last successful reauthentication occurred less than 60 seconds
// before the current attempt.
TEST_F(ReauthenticationModuleTest, ReauthReusePermittedLessThanSixtySeconds) {
  // Pretends authentication was successful 20 seconds ago.
  [reauth_module_ setLastSuccessfulReauthTime:clock_.Now()];
  clock_.Advance(base::Seconds(20));

  [[auth_context_ reject] evaluatePolicy:LAPolicyDeviceOwnerAuthentication
                         localizedReason:[OCMArg any]
                                   reply:[OCMArg any]];

  // Use @try/@catch as -reject raises an exception.
  @try {
    base::RunLoop run_loop;
    [reauth_module_ attemptReauthWithLocalizedReason:@"Test"
                                canReusePreviousAuth:YES
                                             handler:CallQuitClosure(run_loop)];

    run_loop.Run();

    EXPECT_OCMOCK_VERIFY(auth_context_);
  } @catch (NSException* exception) {
    // The exception is raised when
    // - attemptReauthWithLocalizedReason:canReusePreviousAuth:handler:
    // is invoked. As this should not happen, mark the test as failed.
    GTEST_FAIL();
  }
}

// Tests that the previous reauthentication is not reused when reuse is
// permitted, but the last successful reauthentication occurred more than 60
// seconds before the current attempt.
TEST_F(ReauthenticationModuleTest, ReauthReusePermittedMoreThanSixtySeconds) {
  // Pretends authentication was successful 70 seconds ago.
  [reauth_module_ setLastSuccessfulReauthTime:clock_.Now()];
  clock_.Advance(base::Seconds(70));

  OCMExpect([auth_context_ evaluatePolicy:LAPolicyDeviceOwnerAuthentication
                          localizedReason:[OCMArg any]
                                    reply:[OCMArg any]])
      .andDo(^(NSInvocation* invocation) {
        // Use -getArgument:atIndex: to access the block passed by the
        // implementation of ReauthenticationModule and invoke it. The
        // first two arguments are `self` and `cmd` so the index needs
        // to start at 2, thus 4 for the `reply` parameter.
        __unsafe_unretained void (^block)(BOOL, NSError*) = nil;
        [invocation getArgument:&block atIndex:4];
        block(YES, nil);
      });

  base::RunLoop run_loop;
  [reauth_module_ attemptReauthWithLocalizedReason:@"Test"
                              canReusePreviousAuth:YES
                                           handler:CallQuitClosure(run_loop)];
  run_loop.Run();

  EXPECT_OCMOCK_VERIFY(auth_context_);
}

}  // namespace
