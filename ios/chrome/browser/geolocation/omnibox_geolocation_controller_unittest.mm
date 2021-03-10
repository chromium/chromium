// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/geolocation/omnibox_geolocation_controller.h"

#import <CoreLocation/CoreLocation.h>

#import "ios/chrome/browser/geolocation/omnibox_geolocation_controller+Testing.h"
#import "ios/chrome/browser/geolocation/omnibox_geolocation_local_state.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class OmniboxGeolocationControllerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    local_state_ = [[OmniboxGeolocationLocalState alloc] init];

    controller_ = [OmniboxGeolocationController sharedInstance];
    [controller_ setLocalState:local_state_];
  }

  IOSChromeScopedTestingLocalState testing_local_state_;
  OmniboxGeolocationLocalState* local_state_;

  OmniboxGeolocationController* controller_;
};

TEST_F(OmniboxGeolocationControllerTest, TriggerSystemPromptForNewUser) {
  __block BOOL requested = NO;
  __block BOOL enabled = NO;

  id locationManagerMock = OCMClassMock([CLLocationManager class]);
  OCMStub([locationManagerMock requestWhenInUseAuthorization])
      .andDo(^(NSInvocation* invocation) {
        requested = YES;
      });

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunguarded-availability-new"
  // |authorizationStatus| was a class method but got deprecated in favor of an
  // instance method in iOS 14.
  OCMStub(ClassMethod([locationManagerMock authorizationStatus]))
      .andReturn(kCLAuthorizationStatusNotDetermined);
#pragma GCC diagnostic pop

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  // |locationServicesEnabled| is deprecated as an instance method in favor of
  // the class method. Even with ClassMethod, it is still treated as an instance
  // method here. Remove deprecation warning to ensure it is working.
  OCMStub(ClassMethod([locationManagerMock locationServicesEnabled]))
      .andDo(^(NSInvocation* invocation) {
        [invocation setReturnValue:&enabled];
      });
#pragma GCC diagnostic pop

  [controller_ setLocationManager:locationManagerMock];

  // Don't present system prompt if the user has disabled location services.
  [controller_ triggerSystemPromptForNewUser:YES];
  EXPECT_FALSE(requested);

  // Show the system prompt if the user enabled the location service.
  enabled = YES;
  [controller_ triggerSystemPromptForNewUser:YES];
  EXPECT_TRUE(requested);
}

}  // namespace
