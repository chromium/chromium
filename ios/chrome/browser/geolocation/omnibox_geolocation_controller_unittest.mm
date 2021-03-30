// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/geolocation/omnibox_geolocation_controller.h"

#import <CoreLocation/CoreLocation.h>

#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface OmniboxGeolocationController (Testing)
// Sets the LocationManager for the receiver to use.
- (void)setLocationManager:(CLLocationManager*)locationManager;
@end

namespace {

using OmniboxGeolocationControllerTest = PlatformTest;
TEST_F(OmniboxGeolocationControllerTest, TriggerSystemPromptForNewUser) {
  OmniboxGeolocationController* controller =
      [OmniboxGeolocationController sharedInstance];
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

  [controller setLocationManager:locationManagerMock];

  // Don't present system prompt if the user has disabled location services.
  [controller triggerSystemPrompt];
  EXPECT_FALSE(requested);

  // Show the system prompt if the user enabled the location service.
  enabled = YES;
  [controller triggerSystemPrompt];
  EXPECT_TRUE(requested);
}

}  // namespace
