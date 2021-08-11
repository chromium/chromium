// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/geolocation/omnibox_geolocation_controller.h"

#import <CoreLocation/CoreLocation.h>

#include "ios/testing/scoped_block_swizzler.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

typedef void (^RequestBlock)(id self);
typedef CLAuthorizationStatus (^AuthorizationBlock)(id self);
typedef BOOL (^LocationEnabledBlock)(id self);

using OmniboxGeolocationControllerTest = PlatformTest;

// TODO(crbug.com/1238579): Fails on device.
#if TARGET_OS_SIMULATOR
#define MAYBE_TriggerSystemPromptForNewUser TriggerSystemPromptForNewUser
#else
#define MAYBE_TriggerSystemPromptForNewUser \
  DISABLED_TriggerSystemPromptForNewUser
#endif
TEST_F(OmniboxGeolocationControllerTest, MAYBE_TriggerSystemPromptForNewUser) {
  OmniboxGeolocationController* controller =
      [OmniboxGeolocationController sharedInstance];
  __block BOOL requested = NO;
  __block BOOL enabled = NO;
  RequestBlock request_swizzler_block = ^(id self) {
    requested = YES;
  };
  auto request_swizzler = std::make_unique<ScopedBlockSwizzler>(
      [CLLocationManager class], @selector(requestWhenInUseAuthorization),
      request_swizzler_block);

  AuthorizationBlock authorization_swizzler_block = ^(id self) {
    return kCLAuthorizationStatusNotDetermined;
  };
  auto authorization_swizzler = std::make_unique<ScopedBlockSwizzler>(
      [CLLocationManager class], @selector(authorizationStatus),
      authorization_swizzler_block, YES);

  LocationEnabledBlock location_enabled_swizzler_block = ^(id self) {
    return enabled;
  };
  auto location_enabled_swizzler = std::make_unique<ScopedBlockSwizzler>(
      [CLLocationManager class], @selector(locationServicesEnabled),
      location_enabled_swizzler_block, YES);

  // Don't present system prompt if the user has disabled location services.
  [controller triggerSystemPrompt];
  EXPECT_FALSE(requested);

  // Show the system prompt if the user enabled the location service.
  enabled = YES;
  [controller triggerSystemPrompt];
  EXPECT_TRUE(requested);
}

}  // namespace
