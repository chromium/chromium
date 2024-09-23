// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/reauthentication/reauthentication_module_for_testing.h"

#import <LocalAuthentication/LocalAuthentication.h>

#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

@interface TestingSuccessfulReauthTimeAccessor
    : NSObject <SuccessfulReauthTimeAccessor> {
  // Object storing the time of a fake previous successful re-authentication
  // to be used by the `ReauthenticationModule`.
  NSDate* _successfulReauthTime;
}

@end

@implementation TestingSuccessfulReauthTimeAccessor

- (void)updateSuccessfulReauthTime {
  _successfulReauthTime = [[NSDate alloc] init];
}

- (void)updateSuccessfulReauthTime:(NSDate*)time {
  _successfulReauthTime = time;
}

- (NSDate*)lastSuccessfulReauthTime {
  return _successfulReauthTime;
}

@end

namespace {

class ReauthenticationModuleTest : public PlatformTest {
 protected:
  ReauthenticationModuleTest() {}

  void SetUp() override {
    auth_context_ = [OCMockObject niceMockForClass:[LAContext class]];
    time_accessor_ = [[TestingSuccessfulReauthTimeAccessor alloc] init];
    reauthentication_module_ = [[ReauthenticationModule alloc]
        initWithSuccessfulReauthTimeAccessor:time_accessor_];
    [reauthentication_module_ setCreateLAContext:^LAContext*() {
      return auth_context_;
    }];
  }

  id auth_context_;
  TestingSuccessfulReauthTimeAccessor* time_accessor_;
  ReauthenticationModule* reauthentication_module_;
};

// Tests that reauthentication is not reused when reuse is not permitted
// even if the time interval since the previous reauthentication is less
// than 60 seconds.
// TODO(crbug.com/40167264): The test fails on device.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_ReauthReuseNotPermitted ReauthReuseNotPermitted
#else
#define MAYBE_ReauthReuseNotPermitted DISABLED_ReauthReuseNotPermitted
#endif
TEST_F(ReauthenticationModuleTest, MAYBE_ReauthReuseNotPermitted) {
  const int kIntervalFromFakePreviousAuthInSeconds = 20;
  NSDate* lastReauthTime = [NSDate date];
  [time_accessor_ updateSuccessfulReauthTime:lastReauthTime];
  NSDate* newReauthTime =
      [NSDate dateWithTimeInterval:kIntervalFromFakePreviousAuthInSeconds
                         sinceDate:lastReauthTime];

  id nsDateMock = OCMClassMock([NSDate class]);
  OCMStub([nsDateMock date]).andReturn(newReauthTime);

  OCMExpect([auth_context_ evaluatePolicy:LAPolicyDeviceOwnerAuthentication
                          localizedReason:[OCMArg any]
                                    reply:[OCMArg any]]);
  [reauthentication_module_
      attemptReauthWithLocalizedReason:@"Test"
                  canReusePreviousAuth:NO
                               handler:^(ReauthenticationResult success){
                               }];

  EXPECT_OCMOCK_VERIFY(auth_context_);
}

// Tests that the previous reauthentication is reused when reuse is permitted
// and the last successful reauthentication occured less than 60 seconds
// before the current attempt.
// TODO(crbug.com/40167264): The test fails on device.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_ReauthReusePermittedLessThanSixtySeconds \
  ReauthReusePermittedLessThanSixtySeconds
#else
#define MAYBE_ReauthReusePermittedLessThanSixtySeconds \
  DISABLED_ReauthReusePermittedLessThanSixtySeconds
#endif
TEST_F(ReauthenticationModuleTest,
       MAYBE_ReauthReusePermittedLessThanSixtySeconds) {
  const int kIntervalFromFakePreviousAuthInSeconds = 20;
  NSDate* lastReauthTime = [NSDate date];
  [time_accessor_ updateSuccessfulReauthTime:lastReauthTime];
  NSDate* newReauthTime =
      [NSDate dateWithTimeInterval:kIntervalFromFakePreviousAuthInSeconds
                         sinceDate:lastReauthTime];

  id nsDateMock = OCMClassMock([NSDate class]);
  OCMStub([nsDateMock date]).andReturn(newReauthTime);
  [[auth_context_ reject] evaluatePolicy:LAPolicyDeviceOwnerAuthentication
                         localizedReason:[OCMArg any]
                                   reply:[OCMArg any]];

  // Use @try/@catch as -reject raises an exception.
  @try {
    [reauthentication_module_
        attemptReauthWithLocalizedReason:@"Test"
                    canReusePreviousAuth:YES
                                 handler:^(ReauthenticationResult success){
                                 }];
    EXPECT_OCMOCK_VERIFY(auth_context_);
  } @catch (NSException* exception) {
    // The exception is raised when
    // - attemptReauthWithLocalizedReason:canReusePreviousAuth:handler:
    // is invoked. As this should not happen, mark the test as failed.
    GTEST_FAIL();
  }
}

// Tests that the previous reauthentication is not reused when reuse is
// permitted, but the last successful reauthentication occured more than 60
// seconds before the current attempt.
// TODO(crbug.com/40167264): The test fails on device.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_ReauthReusePermittedMoreThanSixtySeconds \
  ReauthReusePermittedMoreThanSixtySeconds
#else
#define MAYBE_ReauthReusePermittedMoreThanSixtySeconds \
  DISABLED_ReauthReusePermittedMoreThanSixtySeconds
#endif
TEST_F(ReauthenticationModuleTest,
       MAYBE_ReauthReusePermittedMoreThanSixtySeconds) {
  const int kIntervalFromFakePreviousAuthInSeconds = 70;
  NSDate* lastReauthTime = [NSDate date];
  [time_accessor_ updateSuccessfulReauthTime:lastReauthTime];
  NSDate* newReauthTime =
      [NSDate dateWithTimeInterval:kIntervalFromFakePreviousAuthInSeconds
                         sinceDate:lastReauthTime];

  id nsDateMock = OCMClassMock([NSDate class]);
  OCMStub([nsDateMock date]).andReturn(newReauthTime);

  OCMExpect([auth_context_ evaluatePolicy:LAPolicyDeviceOwnerAuthentication
                          localizedReason:[OCMArg any]
                                    reply:[OCMArg any]]);
  [reauthentication_module_
      attemptReauthWithLocalizedReason:@"Test"
                  canReusePreviousAuth:YES
                               handler:^(ReauthenticationResult success){
                               }];

  EXPECT_OCMOCK_VERIFY(auth_context_);
}

}  // namespace
