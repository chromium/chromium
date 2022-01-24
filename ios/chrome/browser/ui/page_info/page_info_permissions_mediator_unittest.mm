// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/page_info/page_info_permissions_mediator.h"

#include "base/test/scoped_feature_list.h"
#include "ios/web/common/features.h"
#import "ios/web/public/permissions/permissions.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Tests for Permissions mediator for the page info.
class PageInfoPermissionsTest : public PlatformTest {
 protected:
  PageInfoPermissionsTest() {
    feature_list_.InitAndEnableFeature(web::features::kMediaPermissionsControl);
  }

  void SetUp() override {
    PlatformTest::SetUp();

    if (@available(iOS 15.0, *)) {
      fake_web_state_ = std::make_unique<web::FakeWebState>();
      web::WebState* web_state_ = fake_web_state_.get();

      // Initialize camera state to Allowed but keeps microphone state
      // NotAccessible.
      web_state_->SetStateForPermission(web::PermissionStateAllowed,
                                        web::PermissionCamera);
      web_state_->SetStateForPermission(web::PermissionStateNotAccessible,
                                        web::PermissionMicrophone);

      mediator_ =
          [[PageInfoPermissionsMediator alloc] initWithWebState:web_state_];
    }
  }

  PageInfoPermissionsMediator* mediator() API_AVAILABLE(ios(15.0)) {
    return mediator_;
  }

  web::WebState* web_state() { return fake_web_state_.get(); }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<web::FakeWebState> fake_web_state_;
  PageInfoPermissionsMediator* mediator_ API_AVAILABLE(ios(15.0));
};

// Verifies that |shouldShowPermissionsSection| returns YES if any permission is
// accessible, and returns NO if no permission is.
TEST_F(PageInfoPermissionsTest, TestShouldShowPermissionsSection) {
  if (@available(iOS 15.0, *)) {
    ASSERT_TRUE([mediator() shouldShowPermissionsSection]);

    // Initialize another web state with no accessible permissions.
    auto web_state_no_permission = std::make_unique<web::FakeWebState>();
    PageInfoPermissionsMediator* mediator_no_permission =
        [[PageInfoPermissionsMediator alloc]
            initWithWebState:web_state_no_permission.get()];
    ASSERT_FALSE([mediator_no_permission shouldShowPermissionsSection]);
  }
}

// Verifies that |isPermissionAccessible| returns the right result.
TEST_F(PageInfoPermissionsTest, TestIsPermissionAccessible) {
  if (@available(iOS 15.0, *)) {
    ASSERT_TRUE([mediator() isPermissionAccessible:web::PermissionCamera]);
    ASSERT_FALSE([mediator() isPermissionAccessible:web::PermissionMicrophone]);
  }
}

// Verifies that |stateForAccessiblePermission:| returns the right result.
TEST_F(PageInfoPermissionsTest, TestStateForAccessiblePermission) {
  if (@available(iOS 15.0, *)) {
    ASSERT_TRUE(
        [mediator() stateForAccessiblePermission:web::PermissionCamera]);
  }
}

// Verifies that |toggleStateForPermission:| toggles the web state permission.
TEST_F(PageInfoPermissionsTest, TestToggleStateForPermission) {
  if (@available(iOS 15.0, *)) {
    [mediator() toggleStateForPermission:web::PermissionCamera];
    ASSERT_FALSE(
        [mediator() stateForAccessiblePermission:web::PermissionCamera]);
    ASSERT_EQ(web_state()->GetStateForPermission(web::PermissionCamera),
              web::PermissionStateBlocked);
  }
}
