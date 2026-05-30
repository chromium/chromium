// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_model_delegate_ios.h"

#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

@interface FakeLocationBarModelDelegateWebStateProvider
    : NSObject <LocationBarModelDelegateWebStateProvider>
@property(nonatomic, assign) web::WebState* webState;
@end

@implementation FakeLocationBarModelDelegateWebStateProvider
- (web::WebState*)webStateForLocationBarModelDelegate:
    (const LocationBarModelDelegateIOS*)locationBarModelDelegate {
  return self.webState;
}
@end

class LocationBarModelDelegateIOSTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    profile_ = TestProfileIOS::Builder().Build();
    provider_ = [[FakeLocationBarModelDelegateWebStateProvider alloc] init];
    delegate_ = std::make_unique<LocationBarModelDelegateIOS>(provider_,
                                                              profile_.get());
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  FakeLocationBarModelDelegateWebStateProvider* provider_;
  std::unique_ptr<LocationBarModelDelegateIOS> delegate_;
};

// Tests that IsOfflinePage() returns false if there is no WebState.
TEST_F(LocationBarModelDelegateIOSTest, IsOfflinePage_NoWebState) {
  provider_.webState = nullptr;
  EXPECT_FALSE(delegate_->IsOfflinePage());
}

// Tests that IsOfflinePage() returns false if the WebState has no
// OfflinePageTabHelper attached (to verify defensive null check for
// b/505753157).
TEST_F(LocationBarModelDelegateIOSTest, IsOfflinePage_NoHelper) {
  web::FakeWebState web_state;
  provider_.webState = &web_state;
  // Without OfflinePageTabHelper attached, this should return false and not
  // crash.
  EXPECT_FALSE(delegate_->IsOfflinePage());
}
