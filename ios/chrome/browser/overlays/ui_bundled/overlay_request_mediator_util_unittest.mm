// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_mediator_util.h"

#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_support.h"
#import "ios/chrome/browser/overlays/model/test/overlay_test_macros.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_mediator.h"
#import "testing/platform_test.h"

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
