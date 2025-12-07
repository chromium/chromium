// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/model/tabs_dependency_installer_bridge.h"

#import <memory>

#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

// Test object for tracking calls through the DpendencyInstalling protocol.
@interface TestInstaller : NSObject <TabsDependencyInstalling>

@property(nonatomic) NSInteger insertedCount;
@property(nonatomic) NSInteger removedCount;
@property(nonatomic) NSInteger deletedCount;
@property(nonatomic) NSInteger activatedCount;

@end

@implementation TestInstaller
- (void)webStateInserted:(web::WebState*)webState {
  _insertedCount++;
}
- (void)webStateRemoved:(web::WebState*)webState {
  _removedCount++;
}
- (void)webStateDeleted:(web::WebState*)webState {
  _deletedCount++;
}
- (void)newWebStateActivated:(web::WebState*)newActive
           oldActiveWebState:(web::WebState*)oldActive {
  _activatedCount++;
}
@end

class TabsDependencyInstallerBridgeTest : public PlatformTest {
 public:
  TabsDependencyInstallerBridgeTest()
      : installer_([[TestInstaller alloc] init]) {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(
        profile_.get(), std::make_unique<FakeWebStateListDelegate>());
    web_state_list_ = browser_->GetWebStateList();
  }

  ~TabsDependencyInstallerBridgeTest() override { bridge_.StopObserving(); }

 protected:
  web::WebTaskEnvironment task_environment_;

  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<Browser> browser_;
  TabsDependencyInstallerBridge bridge_;
  TestInstaller* installer_;
  raw_ptr<WebStateList> web_state_list_;
};

// Test that inserting and replacing web states calls the dependency installer
// the expected number of times.
TEST_F(TabsDependencyInstallerBridgeTest, InsertReplaceAndRemoveWebState) {
  bridge_.StartObserving(installer_, browser_.get(),
                         TabsDependencyInstaller::Policy::kOnlyRealized);
  auto web_state_1 = std::make_unique<web::FakeWebState>();
  web_state_list_->InsertWebState(
      std::move(web_state_1),
      WebStateList::InsertionParams::Automatic().Activate());
  EXPECT_EQ(installer_.insertedCount, 1);
  auto web_state_2 = std::make_unique<web::FakeWebState>();
  web_state_list_->ReplaceWebStateAt(0, std::move(web_state_2));
  EXPECT_EQ(installer_.insertedCount, 2);
  EXPECT_EQ(installer_.removedCount, 1);
}

// Tests that closing web states calls the dependency installed as expected.
TEST_F(TabsDependencyInstallerBridgeTest, DeleteWebState) {
  bridge_.StartObserving(installer_, browser_.get(),
                         TabsDependencyInstaller::Policy::kOnlyRealized);

  auto web_state = std::make_unique<web::FakeWebState>();
  web_state_list_->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::Automatic().Activate());
  EXPECT_EQ(installer_.deletedCount, 0);

  web_state_list_->CloseWebStateAt(0, WebStateList::ClosingReason::kUserAction);
  EXPECT_EQ(installer_.deletedCount, 1);
}

// Tests that changing the active web states calls the dependency installer
// as expected.
TEST_F(TabsDependencyInstallerBridgeTest, ActivateWebState) {
  bridge_.StartObserving(installer_, browser_.get(),
                         TabsDependencyInstaller::Policy::kOnlyRealized);

  auto web_state_1 = std::make_unique<web::FakeWebState>();
  web_state_list_->InsertWebState(
      std::move(web_state_1),
      WebStateList::InsertionParams::Automatic().Activate());
  EXPECT_EQ(installer_.activatedCount, 1);

  auto web_state_2 = std::make_unique<web::FakeWebState>();
  web_state_list_->InsertWebState(
      std::move(web_state_2),
      WebStateList::InsertionParams::Automatic().Activate());
  EXPECT_EQ(installer_.activatedCount, 2);

  web_state_list_->ActivateWebStateAt(0);
  EXPECT_EQ(installer_.activatedCount, 3);

  // If the active web state does not change, the installer is not notified.
  web_state_list_->ActivateWebStateAt(0);
  EXPECT_EQ(installer_.activatedCount, 3);
}

// Tests that stopping the observation removes all web states.
TEST_F(TabsDependencyInstallerBridgeTest, RemoveOnBridgeDestruction) {
  bridge_.StartObserving(installer_, browser_.get(),
                         TabsDependencyInstaller::Policy::kOnlyRealized);
  auto web_state_1 = std::make_unique<web::FakeWebState>();
  web_state_list_->InsertWebState(
      std::move(web_state_1),
      WebStateList::InsertionParams::Automatic().Activate());
  auto web_state_2 = std::make_unique<web::FakeWebState>();
  web_state_list_->InsertWebState(
      std::move(web_state_2),
      WebStateList::InsertionParams::Automatic().Activate());
  EXPECT_EQ(installer_.insertedCount, 2);
  EXPECT_EQ(installer_.removedCount, 0);
  bridge_.StopObserving();
  EXPECT_EQ(installer_.removedCount, 2);
}
