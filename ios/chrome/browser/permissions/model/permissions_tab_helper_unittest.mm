// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/permissions/model/permissions_tab_helper.h"

#import "base/test/task_environment.h"
#import "base/threading/platform_thread.h"
#import "base/time/time.h"
#import "components/infobars/core/infobar_manager.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/model/overlays/default_infobar_overlay_request_factory.h"
#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_request_inserter.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_queue.h"
#import "ios/chrome/browser/permissions/model/permissions_infobar_delegate.h"
#import "ios/web/public/permissions/permissions.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {
constexpr base::TimeDelta kTimeoutDelay = base::Milliseconds(251);
}  // namespace

// Test fixture for PermissionsTabHelper.
class PermissionsTabHelperTest : public PlatformTest {
 public:
  PermissionsTabHelperTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    web_state_.SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    OverlayRequestQueue::CreateForWebState(&web_state_);
    InfoBarManagerImpl::CreateForWebState(&web_state_);
    InfobarOverlayRequestInserter::CreateForWebState(
        &web_state_, &DefaultInfobarOverlayRequestFactory);
    PermissionsTabHelper::CreateForWebState(&web_state_);
  }

  ~PermissionsTabHelperTest() override {
    InfoBarManagerImpl::FromWebState(&web_state_)->ShutDown();
    web_state_.RemoveObserver(PermissionsTabHelper::FromWebState(&web_state_));
  }

 protected:
  // Returns InfoBarManager attached to `web_state()`.
  infobars::InfoBarManager* infobar_manager() {
    return InfoBarManagerImpl::FromWebState(&web_state_);
  }

  // Returns the current infobar, if available.
  InfoBarIOS* infobar() {
    return infobar_manager()->infobars().empty()
               ? nullptr
               : static_cast<InfoBarIOS*>(infobar_manager()->infobars()[0]);
  }

  // Returns recently_accessible_permissions determined by the tab helper.
  NSArray<NSNumber*>* recently_accessible_permissions() {
    if (infobar() == nullptr) {
      return [NSArray array];
    }
    PermissionsInfobarDelegate* delegate =
        static_cast<PermissionsInfobarDelegate*>(infobar()->delegate());
    return delegate->GetMostRecentlyAccessiblePermissions();
  }

  base::test::TaskEnvironment task_environment_;
  web::FakeWebState web_state_;
};

// Tests that an infobar is setup with the correct acceptance state when the
// status of a single permission changes.
TEST_F(PermissionsTabHelperTest, CheckInfobarCountForSinglePermission) {
  // Allowed permission.
  web_state_.SetStateForPermission(web::PermissionStateAllowed,
                                   web::PermissionCamera);
  EXPECT_EQ(0U, infobar_manager()->infobars().size());
  task_environment_.FastForwardBy(kTimeoutDelay);
  EXPECT_EQ(1U, infobar_manager()->infobars().size());
  EXPECT_TRUE(infobar()->accepted());
  EXPECT_EQ(1U, [recently_accessible_permissions() count]);
  EXPECT_EQ(recently_accessible_permissions()[0].unsignedIntegerValue,
            web::PermissionCamera);
  // Blocked permission.
  web_state_.SetStateForPermission(web::PermissionStateBlocked,
                                   web::PermissionCamera);
  EXPECT_EQ(1U, infobar_manager()->infobars().size());
  EXPECT_FALSE(infobar()->accepted());
  // Permission not accessible.
  web_state_.SetStateForPermission(web::PermissionStateNotAccessible,
                                   web::PermissionCamera);
  EXPECT_EQ(0U, infobar_manager()->infobars().size());
  EXPECT_EQ(0U, [recently_accessible_permissions() count]);
}

// Tests that blocking a permission and allowing it again correctly resets
// infobar and acceptance state.
TEST_F(PermissionsTabHelperTest, BlockingAndAllowingSinglePermission) {
  // Allowed permission.
  web_state_.SetStateForPermission(web::PermissionStateAllowed,
                                   web::PermissionMicrophone);
  EXPECT_EQ(0U, infobar_manager()->infobars().size());
  task_environment_.FastForwardBy(kTimeoutDelay);
  EXPECT_EQ(1U, infobar_manager()->infobars().size());
  EXPECT_TRUE(infobar()->accepted());
  EXPECT_EQ(1U, [recently_accessible_permissions() count]);
  EXPECT_EQ(recently_accessible_permissions()[0].unsignedIntegerValue,
            web::PermissionMicrophone);
  // Block this permission.
  web_state_.SetStateForPermission(web::PermissionStateBlocked,
                                   web::PermissionMicrophone);
  EXPECT_EQ(1U, infobar_manager()->infobars().size());
  EXPECT_FALSE(infobar()->accepted());
  EXPECT_EQ(1U, [recently_accessible_permissions() count]);
  EXPECT_EQ(recently_accessible_permissions()[0].unsignedIntegerValue,
            web::PermissionMicrophone);
  // Allow it again.
  web_state_.SetStateForPermission(web::PermissionStateAllowed,
                                   web::PermissionMicrophone);
  // The tab helper should not have to wait for the timeout.
  EXPECT_EQ(1U, infobar_manager()->infobars().size());
  EXPECT_TRUE(infobar()->accepted());
  EXPECT_EQ(1U, [recently_accessible_permissions() count]);
  EXPECT_EQ(recently_accessible_permissions()[0].unsignedIntegerValue,
            web::PermissionMicrophone);
}

// Tests that making a permission inaccessible and allowing it again correctly
// resets infobar and acceptance state.
TEST_F(PermissionsTabHelperTest,
       MakingPermissionNotAccessibleAndAllowingItAgain) {
  // Allowed permission.
  web_state_.SetStateForPermission(web::PermissionStateAllowed,
                                   web::PermissionCamera);
  EXPECT_EQ(0U, infobar_manager()->infobars().size());
  task_environment_.FastForwardBy(kTimeoutDelay);
  EXPECT_EQ(1U, infobar_manager()->infobars().size());
  EXPECT_TRUE(infobar()->accepted());
  EXPECT_EQ(1U, [recently_accessible_permissions() count]);
  EXPECT_EQ(recently_accessible_permissions()[0].unsignedIntegerValue,
            web::PermissionCamera);
  // Make this permission inaccessible.
  web_state_.SetStateForPermission(web::PermissionStateNotAccessible,
                                   web::PermissionCamera);
  EXPECT_EQ(0U, infobar_manager()->infobars().size());
  // Access it again.
  web_state_.SetStateForPermission(web::PermissionStateAllowed,
                                   web::PermissionCamera);
  // The tab helper should wait for the timeout again to create the infobar.
  EXPECT_EQ(0U, infobar_manager()->infobars().size());
  task_environment_.FastForwardBy(kTimeoutDelay);
  EXPECT_EQ(1U, infobar_manager()->infobars().size());
  EXPECT_TRUE(infobar()->accepted());
  EXPECT_EQ(1U, [recently_accessible_permissions() count]);
  EXPECT_EQ(recently_accessible_permissions()[0].unsignedIntegerValue,
            web::PermissionCamera);
}

// Tests that an infobar is setup with the correct acceptance state when the
// status of both permission are allowed simultaneously.
TEST_F(PermissionsTabHelperTest,
       CheckInfobarCountForSimultaneouslyAllowedPermissions) {
  // Allow both permissions.
  web_state_.SetStateForPermission(web::PermissionStateAllowed,
                                   web::PermissionCamera);
  web_state_.SetStateForPermission(web::PermissionStateAllowed,
                                   web::PermissionMicrophone);
  EXPECT_EQ(0U, infobar_manager()->infobars().size());
  task_environment_.FastForwardBy(kTimeoutDelay);
  EXPECT_EQ(1U, infobar_manager()->infobars().size());
  EXPECT_TRUE(infobar()->accepted());
  EXPECT_EQ(2U, [recently_accessible_permissions() count]);
  EXPECT_EQ(recently_accessible_permissions()[0].unsignedIntegerValue,
            web::PermissionCamera);
  EXPECT_EQ(recently_accessible_permissions()[1].unsignedIntegerValue,
            web::PermissionMicrophone);
  // Blocked one of the permissions.
  web_state_.SetStateForPermission(web::PermissionStateBlocked,
                                   web::PermissionCamera);
  EXPECT_EQ(1U, infobar_manager()->infobars().size());
  EXPECT_TRUE(infobar()->accepted());
  // Now block the other.
  web_state_.SetStateForPermission(web::PermissionStateBlocked,
                                   web::PermissionMicrophone);
  EXPECT_EQ(1U, infobar_manager()->infobars().size());
  EXPECT_FALSE(infobar()->accepted());
  // Make one of the permissions not accessible. Infobar should still exist.
  web_state_.SetStateForPermission(web::PermissionStateNotAccessible,
                                   web::PermissionMicrophone);
  EXPECT_EQ(1U, infobar_manager()->infobars().size());
  EXPECT_FALSE(infobar()->accepted());
  // Make the other not accessible too. Infobar should be removed.
  web_state_.SetStateForPermission(web::PermissionStateNotAccessible,
                                   web::PermissionCamera);
  EXPECT_EQ(0U, infobar_manager()->infobars().size());
}

// Tests that an infobar is setup and replaced with the correct acceptance
// state when the status of both permission are allowed one by one.
TEST_F(PermissionsTabHelperTest,
       CheckInfobarCountForSeparatelyAllowedPermissions) {
  // Allow one permission.
  web_state_.SetStateForPermission(web::PermissionStateAllowed,
                                   web::PermissionCamera);
  task_environment_.FastForwardBy(kTimeoutDelay);
  EXPECT_EQ(1U, infobar_manager()->infobars().size());
  EXPECT_EQ(1U, [recently_accessible_permissions() count]);
  EXPECT_EQ(recently_accessible_permissions()[0].unsignedIntegerValue,
            web::PermissionCamera);
  InfoBarIOS* first_infobar = infobar();
  // Now allow the other.
  web_state_.SetStateForPermission(web::PermissionStateAllowed,
                                   web::PermissionMicrophone);
  task_environment_.FastForwardBy(kTimeoutDelay);
  // Check that infobar is properly replaced.
  EXPECT_EQ(1U, infobar_manager()->infobars().size());
  EXPECT_NE(infobar(), first_infobar);
  EXPECT_TRUE(infobar()->accepted());
  EXPECT_EQ(1U, [recently_accessible_permissions() count]);
  EXPECT_EQ(recently_accessible_permissions()[0].unsignedIntegerValue,
            web::PermissionMicrophone);
  // Blocked one of the permissions.
  web_state_.SetStateForPermission(web::PermissionStateBlocked,
                                   web::PermissionMicrophone);
  EXPECT_EQ(1U, infobar_manager()->infobars().size());
  EXPECT_TRUE(infobar()->accepted());
  // Now block the other.
  web_state_.SetStateForPermission(web::PermissionStateBlocked,
                                   web::PermissionCamera);
  EXPECT_EQ(1U, infobar_manager()->infobars().size());
  EXPECT_FALSE(infobar()->accepted());
  // Make one of the permissions not accessible. Infobar should still exist.
  web_state_.SetStateForPermission(web::PermissionStateNotAccessible,
                                   web::PermissionMicrophone);
  EXPECT_EQ(1U, infobar_manager()->infobars().size());
  EXPECT_FALSE(infobar()->accepted());
  // Make the other not accessible too. Infobar should be removed.
  web_state_.SetStateForPermission(web::PermissionStateNotAccessible,
                                   web::PermissionCamera);
  EXPECT_EQ(0U, infobar_manager()->infobars().size());
}

// Tests that infobar and acceptance state would be handled correctly when one
// permission is blocked while the other permission changes states.
TEST_F(PermissionsTabHelperTest,
       CheckInfobarAndAcceptanceStateWhenOnePermissionIsBlocked) {
  // Allow one permission.
  web_state_.SetStateForPermission(web::PermissionStateAllowed,
                                   web::PermissionCamera);
  web_state_.SetStateForPermission(web::PermissionStateAllowed,
                                   web::PermissionMicrophone);
  task_environment_.FastForwardBy(kTimeoutDelay);
  ASSERT_EQ(2U, [recently_accessible_permissions() count]);
  EXPECT_EQ(recently_accessible_permissions()[0].unsignedIntegerValue,
            web::PermissionCamera);
  EXPECT_EQ(recently_accessible_permissions()[1].unsignedIntegerValue,
            web::PermissionMicrophone);
  // Now block one. The other one is still allowed, so infobar should still be
  // accepted.
  web_state_.SetStateForPermission(web::PermissionStateBlocked,
                                   web::PermissionMicrophone);
  EXPECT_EQ(1U, infobar_manager()->infobars().size());
  EXPECT_TRUE(infobar()->accepted());
  // Now make the other one inaccessible. Infobar should still exist, but its
  // acceptance state should be false.
  web_state_.SetStateForPermission(web::PermissionStateNotAccessible,
                                   web::PermissionCamera);
  EXPECT_EQ(1U, infobar_manager()->infobars().size());
  EXPECT_FALSE(infobar()->accepted());
  // Now make the other one "allowed" again. The acceptance state would be
  // changed immediately, but the infobar would not be replaced until timeout
  // is reached.
  InfoBarIOS* first_infobar = infobar();
  web_state_.SetStateForPermission(web::PermissionStateAllowed,
                                   web::PermissionCamera);
  ASSERT_EQ(1U, infobar_manager()->infobars().size());
  EXPECT_EQ(first_infobar, infobar());
  EXPECT_TRUE(infobar()->accepted());
  // But after the timeout, the infobar should be replaced and have the right
  // acceptance state.
  task_environment_.FastForwardBy(kTimeoutDelay);
  EXPECT_EQ(1U, infobar_manager()->infobars().size());
  EXPECT_NE(first_infobar, infobar());
  EXPECT_TRUE(infobar()->accepted());
  EXPECT_EQ(1U, [recently_accessible_permissions() count]);
  EXPECT_EQ(recently_accessible_permissions()[0].unsignedIntegerValue,
            web::PermissionCamera);
}
