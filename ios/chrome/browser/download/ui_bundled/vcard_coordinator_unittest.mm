// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/ui_bundled/vcard_coordinator.h"

#import "base/test/task_environment.h"
#import "ios/chrome/browser/download/model/vcard_tab_helper.h"
#import "ios/chrome/browser/download/model/vcard_tab_helper_delegate.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/platform_test.h"

// Test fixture for VcardCoordinatorTest class.
class VcardCoordinatorTest : public PlatformTest {
 protected:
  VcardCoordinatorTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    coordinator_ =
        [[VcardCoordinator alloc] initWithBaseViewController:nil
                                                     browser:browser_.get()];
    [scoped_key_window_.Get() setRootViewController:nil];

    [coordinator_ start];
  }

  ~VcardCoordinatorTest() override { [coordinator_ stop]; }

  // Needed for test profile created by TestBrowser().
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  VcardCoordinator* coordinator_;
  ScopedKeyWindow scoped_key_window_;
};

// Tests that the coordinator installs itself as a VcardTabHelper delegate when
// VcardTabHelper instances become available.
TEST_F(VcardCoordinatorTest, InstallDelegates) {
  // Coordinator should install itself as delegate for a new web state.
  auto web_state2 = std::make_unique<web::FakeWebState>();
  auto* web_state_ptr2 = web_state2.get();
  VcardTabHelper::CreateForWebState(web_state_ptr2);
  EXPECT_FALSE(VcardTabHelper::FromWebState(web_state_ptr2)->delegate());
  browser_->GetWebStateList()->InsertWebState(std::move(web_state2));
  EXPECT_TRUE(VcardTabHelper::FromWebState(web_state_ptr2)->delegate());

  // Coordinator should install itself as delegate for a web state replacing an
  // existing one.
  auto web_state3 = std::make_unique<web::FakeWebState>();
  auto* web_state_ptr3 = web_state3.get();
  VcardTabHelper::CreateForWebState(web_state_ptr3);
  EXPECT_FALSE(VcardTabHelper::FromWebState(web_state_ptr3)->delegate());
  browser_->GetWebStateList()->ReplaceWebStateAt(0, std::move(web_state3));
  EXPECT_TRUE(VcardTabHelper::FromWebState(web_state_ptr3)->delegate());
}
