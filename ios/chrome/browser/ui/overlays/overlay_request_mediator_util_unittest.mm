// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/overlay_request_mediator_util.h"

#include "ios/chrome/browser/overlays/public/overlay_request.h"
#include "ios/chrome/browser/overlays/public/overlay_request_support.h"
#include "ios/chrome/browser/overlays/test/overlay_test_macros.h"
#import "ios/chrome/browser/ui/overlays/overlay_request_mediator.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Fake request config types for use in tests.
DEFINE_TEST_OVERLAY_REQUEST_CONFIG(FirstConfig);
DEFINE_TEST_OVERLAY_REQUEST_CONFIG(SecondConfig);
DEFINE_TEST_OVERLAY_REQUEST_CONFIG(ThirdConfig);
}  // namespace

@interface FirstMediator : OverlayRequestMediator
@end

@implementation FirstMediator
+ (const OverlayRequestSupport*)requestSupport {
  return FirstConfig::RequestSupport();
}
@end

@interface SecondMediator : OverlayRequestMediator
@end

@implementation SecondMediator
+ (const OverlayRequestSupport*)requestSupport {
  return SecondConfig::RequestSupport();
}
@end

using OverlayRequestMediatorUtilTest = PlatformTest;

// Tests that CreateAggregateSupportForMediators() supports requests created
// with the configs supported by the mediator classes.
TEST_F(OverlayRequestMediatorUtilTest, CreateAggregateSupportForMediators) {
  std::unique_ptr<OverlayRequest> first_request =
      OverlayRequest::CreateWithConfig<FirstConfig>();
  std::unique_ptr<OverlayRequest> second_request =
      OverlayRequest::CreateWithConfig<SecondConfig>();
  std::unique_ptr<OverlayRequest> third_request =
      OverlayRequest::CreateWithConfig<ThirdConfig>();

  NSArray<Class>* mediator_classes =
      @ [[FirstMediator class], [SecondMediator class]];
  std::unique_ptr<OverlayRequestSupport> support =
      CreateAggregateSupportForMediators(mediator_classes);

  EXPECT_TRUE(support->IsRequestSupported(first_request.get()));
  EXPECT_TRUE(support->IsRequestSupported(second_request.get()));
  EXPECT_FALSE(support->IsRequestSupported(third_request.get()));
}
