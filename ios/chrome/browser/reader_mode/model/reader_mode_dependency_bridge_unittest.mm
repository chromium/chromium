// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/model/reader_mode_dependency_bridge.h"

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/tabs/model/tabs_dependency_installer.h"
#import "ios/chrome/browser/tabs/model/tabs_dependency_installer_manager.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

// Mock TabsDependencyInstaller for testing.
class MockTabsDependencyInstaller : public TabsDependencyInstaller {
 public:
  MockTabsDependencyInstaller() = default;
  ~MockTabsDependencyInstaller() override = default;

  MOCK_METHOD(void,
              OnWebStateInserted,
              (web::WebState * web_state),
              (override));
  MOCK_METHOD(void, OnWebStateRemoved, (web::WebState * web_state), (override));
  MOCK_METHOD(void, OnWebStateDeleted, (web::WebState * web_state), (override));
  MOCK_METHOD(void,
              OnActiveWebStateChanged,
              (web::WebState * old_active, web::WebState* new_active),
              (override));
};

class ReaderModeDependencyBridgeTest : public PlatformTest {
 protected:
  ReaderModeDependencyBridgeTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    TabsDependencyInstallerManager::CreateForBrowser(browser_.get());
    manager_ = TabsDependencyInstallerManager::FromBrowser(browser_.get());
    bridge_ = std::make_unique<ReaderModeDependencyBridge>(browser_.get());
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  raw_ptr<TabsDependencyInstallerManager> manager_;
  std::unique_ptr<ReaderModeDependencyBridge> bridge_;
};

// Tests that ReaderModeWebStateDidLoadContent calls InstallDependencies.
TEST_F(ReaderModeDependencyBridgeTest, WebStateDidLoadContent) {
  MockTabsDependencyInstaller installer;
  manager_->AddInstaller(&installer);
  web::FakeWebState web_state;

  EXPECT_CALL(installer, OnWebStateInserted(&web_state));
  bridge_->ReaderModeWebStateDidLoadContent(&web_state);
}

// Tests that ReaderModeWebStateWillBecomeUnavailable calls
// UninstallDependencies.
TEST_F(ReaderModeDependencyBridgeTest, WebStateWillBecomeUnavailable) {
  MockTabsDependencyInstaller installer;
  manager_->AddInstaller(&installer);
  web::FakeWebState web_state;

  // First, install dependencies.
  bridge_->ReaderModeWebStateDidLoadContent(&web_state);

  EXPECT_CALL(installer, OnWebStateRemoved(&web_state));
  bridge_->ReaderModeWebStateWillBecomeUnavailable(&web_state);
}

// Tests that ReaderModeTabHelperDestroyed calls PurgeDependencies.
TEST_F(ReaderModeDependencyBridgeTest, TabHelperDestroyed) {
  MockTabsDependencyInstaller installer;
  manager_->AddInstaller(&installer);
  web::FakeWebState web_state;

  // First, install dependencies.
  bridge_->ReaderModeWebStateDidLoadContent(&web_state);

  EXPECT_CALL(installer, OnWebStateDeleted(&web_state));
  bridge_->ReaderModeTabHelperDestroyed(&web_state);
}
