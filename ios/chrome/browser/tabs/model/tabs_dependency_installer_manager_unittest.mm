// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/model/tabs_dependency_installer_manager.h"

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/tabs/model/tabs_dependency_installer.h"
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

class TabsDependencyInstallerManagerTest : public PlatformTest {
 protected:
  TabsDependencyInstallerManagerTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    TabsDependencyInstallerManager::CreateForBrowser(browser_.get());
    manager_ = TabsDependencyInstallerManager::FromBrowser(browser_.get());
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  raw_ptr<TabsDependencyInstallerManager> manager_;
};

// Tests that installers are added and removed correctly.
TEST_F(TabsDependencyInstallerManagerTest, AddAndRemoveInstaller) {
  MockTabsDependencyInstaller installer;
  manager_->AddInstaller(&installer);
  web::FakeWebState web_state;

  EXPECT_CALL(installer, OnWebStateInserted(&web_state));
  manager_->InstallDependencies(&web_state);

  manager_->RemoveInstaller(&installer);

  // After removing, the installer should not be called.
  EXPECT_CALL(installer, OnWebStateRemoved(&web_state)).Times(0);
  manager_->UninstallDependencies(&web_state);
}

// Tests that InstallDependencies calls OnWebStateInserted on installers.
TEST_F(TabsDependencyInstallerManagerTest, InstallDependencies) {
  MockTabsDependencyInstaller installer;
  manager_->AddInstaller(&installer);
  web::FakeWebState web_state;

  EXPECT_CALL(installer, OnWebStateInserted(&web_state));
  manager_->InstallDependencies(&web_state);
}

// Tests that UninstallDependencies calls OnWebStateRemoved on installers.
TEST_F(TabsDependencyInstallerManagerTest, UninstallDependencies) {
  MockTabsDependencyInstaller installer;
  manager_->AddInstaller(&installer);
  web::FakeWebState web_state;

  manager_->InstallDependencies(&web_state);

  EXPECT_CALL(installer, OnWebStateRemoved(&web_state));
  manager_->UninstallDependencies(&web_state);
}

// Tests that PurgeDependencies calls OnWebStateDeleted on installers.
TEST_F(TabsDependencyInstallerManagerTest, PurgeDependencies) {
  MockTabsDependencyInstaller installer;
  manager_->AddInstaller(&installer);
  web::FakeWebState web_state;

  manager_->InstallDependencies(&web_state);

  EXPECT_CALL(installer, OnWebStateDeleted(&web_state));
  manager_->PurgeDependencies(&web_state);
}

// Tests the case where an installer is added after dependencies have been
// installed on a WebState.
TEST_F(TabsDependencyInstallerManagerTest, AddInstallerAfterInstall) {
  MockTabsDependencyInstaller installer1;
  manager_->AddInstaller(&installer1);
  web::FakeWebState web_state;

  EXPECT_CALL(installer1, OnWebStateInserted(&web_state));
  manager_->InstallDependencies(&web_state);

  MockTabsDependencyInstaller installer2;
  // When the second installer is added, it should have its dependencies
  // installed on the existing WebState.
  EXPECT_CALL(installer2, OnWebStateInserted(&web_state));
  manager_->AddInstaller(&installer2);
}

// Tests that UninstallDependencies is a no-op if InstallDependencies was not
// called.
TEST_F(TabsDependencyInstallerManagerTest,
       UninstallDependenciesNeverInstalled) {
  MockTabsDependencyInstaller installer;
  manager_->AddInstaller(&installer);
  web::FakeWebState web_state;

  EXPECT_CALL(installer, OnWebStateRemoved(&web_state)).Times(0);
  manager_->UninstallDependencies(&web_state);
}

// Tests that PurgeDependencies is a no-op if InstallDependencies was not
// called.
TEST_F(TabsDependencyInstallerManagerTest, PurgeDependenciesNeverInstalled) {
  MockTabsDependencyInstaller installer;
  manager_->AddInstaller(&installer);
  web::FakeWebState web_state;

  EXPECT_CALL(installer, OnWebStateDeleted(&web_state)).Times(0);
  manager_->PurgeDependencies(&web_state);
}
