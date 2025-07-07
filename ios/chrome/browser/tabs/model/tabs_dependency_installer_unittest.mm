// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/model/tabs_dependency_installer.h"

#import <memory>
#import <set>

#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

// TabsDependencyInstaller which simply tracks which WebStates have been passed
// to the inserted/removed methods.
class FakeDependencyInstaller : public TabsDependencyInstaller {
 public:
  ~FakeDependencyInstaller() override = default;

  // TabsDependencyInstaller implementation.
  void OnWebStateInserted(web::WebState* web_state) override {
    installed_.insert(web_state);
  }
  void OnWebStateRemoved(web::WebState* web_state) override {
    uninstalled_.insert(web_state);
  }

  bool WasInstalled(web::WebState* web_state) const {
    return installed_.count(web_state) > 0;
  }

  bool WasUninstalled(web::WebState* web_state) const {
    return uninstalled_.count(web_state) > 0;
  }

  size_t InstallCount(web::WebState* web_state) const {
    return installed_.count(web_state);
  }

  size_t UninstallCount(web::WebState* web_state) const {
    return uninstalled_.count(web_state);
  }

 private:
  std::set<web::WebState*> installed_;
  std::set<web::WebState*> uninstalled_;
};

}  // anonymous namespace

class TabsDependencyInstallerTest : public PlatformTest {
 public:
  TabsDependencyInstallerTest()
      : web_state_list_delegate_(/* force_realization_on_activation */ true),
        web_state_list_(&web_state_list_delegate_) {}

  ~TabsDependencyInstallerTest() override { installer_.StopObserving(); }

 protected:
  FakeWebStateListDelegate web_state_list_delegate_;
  WebStateList web_state_list_;
  FakeDependencyInstaller installer_;
};

// Verifies that the appropriate inserted/removed methods are triggered
// when a WebState is inserted, replaced, or removed.
TEST_F(TabsDependencyInstallerTest, InsertReplaceAndRemoveWebState) {
  installer_.StartObserving(&web_state_list_);
  auto web_state_1 = std::make_unique<web::FakeWebState>();
  web::WebState* web_state_1_raw = web_state_1.get();

  EXPECT_FALSE(installer_.WasInstalled(web_state_1_raw));
  web_state_list_.InsertWebState(
      std::move(web_state_1),
      WebStateList::InsertionParams::Automatic().Activate());
  EXPECT_TRUE(installer_.WasInstalled(web_state_1_raw));
  EXPECT_FALSE(installer_.WasUninstalled(web_state_1_raw));

  auto web_state_2 = std::make_unique<web::FakeWebState>();
  web::WebState* web_state_2_raw = web_state_2.get();
  web_state_list_.ReplaceWebStateAt(0, std::move(web_state_2));
  EXPECT_TRUE(installer_.WasUninstalled(web_state_1_raw));
  EXPECT_TRUE(installer_.WasInstalled(web_state_2_raw));
}

// Verifies that the appropriate inserted/removed methods are triggered
// for any WebStates that were already in the WebStateList prior to its
// observation.
TEST_F(TabsDependencyInstallerTest, RespectsPreexistingWebState) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web::WebState* web_state_raw = web_state.get();
  web_state_list_.InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::Automatic().Activate());
  installer_.StartObserving(&web_state_list_);
  EXPECT_TRUE(installer_.WasInstalled(web_state_raw));
}

// Verifies that the inserted/removed methods are not triggered for any
// unrealized WebStates.
TEST_F(TabsDependencyInstallerTest, UnrealizedWebStates) {
  installer_.StartObserving(&web_state_list_);
  auto web_state_1 = std::make_unique<web::FakeWebState>();
  web::WebState* web_state_1_raw = web_state_1.get();
  web_state_list_.InsertWebState(
      std::move(web_state_1),
      WebStateList::InsertionParams::Automatic().Activate());
  EXPECT_TRUE(installer_.WasInstalled(web_state_1_raw));

  auto web_state_2 = std::make_unique<web::FakeWebState>();
  web_state_2->SetIsRealized(false);
  web::WebState* web_state_2_raw = web_state_2.get();

  // Insert the unrealized webstate but don't have it activate (since that
  // forces realization).
  web_state_list_.InsertWebState(std::move(web_state_2));
  // The unrealized webstate should not have dependencies installed.
  EXPECT_FALSE(installer_.WasInstalled(web_state_2_raw));
  // Once realized, dependencies should be installed.
  web_state_2_raw->ForceRealized();
  EXPECT_TRUE(installer_.WasInstalled(web_state_2_raw));

  auto web_state_3 = std::make_unique<web::FakeWebState>();
  web_state_3->SetIsRealized(false);
  web::WebState* web_state_3_raw = web_state_3.get();

  // Insert the unrealized webstate and activate it, forcing realization.
  web_state_list_.InsertWebState(
      std::move(web_state_3),
      WebStateList::InsertionParams::Automatic().Activate());
  // The formerly unrealized webstate should have dependencies installed.
  // (The webstate should also be realized, but that's not the responsibility
  // of the code under test).
  EXPECT_TRUE(installer_.WasInstalled(web_state_3_raw));
  // Dependencies should have been installed only once
  EXPECT_EQ(1u, installer_.InstallCount(web_state_3_raw));
}

// Verifies that no methods are triggered after stopping the observation.
TEST_F(TabsDependencyInstallerTest, Disconnect) {
  installer_.StartObserving(&web_state_list_);
  auto web_state_1 = std::make_unique<web::FakeWebState>();
  web::WebState* web_state_1_raw = web_state_1.get();

  web_state_list_.InsertWebState(
      std::move(web_state_1),
      WebStateList::InsertionParams::Automatic().Activate());
  EXPECT_TRUE(installer_.WasInstalled(web_state_1_raw));
  installer_.StopObserving();

  EXPECT_TRUE(installer_.WasUninstalled(web_state_1_raw));
}
