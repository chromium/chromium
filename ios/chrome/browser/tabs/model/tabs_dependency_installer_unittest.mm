// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/model/tabs_dependency_installer.h"

#import <algorithm>
#import <memory>
#import <vector>

#import "base/containers/contains.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/tabs/model/tabs_dependency_installer_manager.h"
#import "ios/web/common/features.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

// Represents the state of the kCreateTabHelperOnlyForRealizedWebStates
// feature.
enum class FeatureState {
  kEnabled,
  kDisabled,
};

// Parameters for TabsDependencyInstallerTest.
using TestParams = std::tuple<FeatureState, TabsDependencyInstaller::Policy>;

// A ScopedFeatureList that enable or disable the feature in its constructor.
class ScopedFeatureListHelper {
 public:
  ScopedFeatureListHelper(FeatureState state) {
    switch (state) {
      case FeatureState::kEnabled:
        scoped_feature_list_.InitAndEnableFeature(
            web::features::kCreateTabHelperOnlyForRealizedWebStates);
        break;

      case FeatureState::kDisabled:
        scoped_feature_list_.InitAndDisableFeature(
            web::features::kCreateTabHelperOnlyForRealizedWebStates);
        break;
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TabsDependencyInstaller which simply tracks which WebStates have been passed
// to the inserted/removed methods.
class TestTabsDependencyInstaller : public TabsDependencyInstaller {
 public:
  // Represents information about a WebStateActivated() event.
  using Activation = std::pair<web::WebStateID, web::WebStateID>;

  ~TestTabsDependencyInstaller() override = default;

  // TabsDependencyInstaller implementation.
  void OnWebStateInserted(web::WebState* web_state) override {
    installed_.push_back(web_state->GetUniqueIdentifier());
  }
  void OnWebStateRemoved(web::WebState* web_state) override {
    uninstalled_.push_back(web_state->GetUniqueIdentifier());
  }
  void OnWebStateDeleted(web::WebState* web_state) override {
    deleted_.push_back(web_state->GetUniqueIdentifier());
  }
  void OnActiveWebStateChanged(web::WebState* old_active,
                               web::WebState* new_active) override {
    activated_.push_back(Activation(
        old_active ? old_active->GetUniqueIdentifier() : web::WebStateID(),
        new_active ? new_active->GetUniqueIdentifier() : web::WebStateID()));
  }

  bool WasInstalled(web::WebStateID web_state_id) const {
    return InstallCount(web_state_id) > 0;
  }

  bool WasUninstalled(web::WebStateID web_state_id) const {
    return UninstallCount(web_state_id) > 0;
  }

  bool WasDeleted(web::WebStateID web_state_id) const {
    return DeletedCount(web_state_id) > 0;
  }

  bool WasActivated(Activation activation) const {
    return ActivatedCount(activation) > 0;
  }

  bool WasActivated(web::WebStateID old_active_id,
                    web::WebStateID new_active_id) const {
    return WasActivated(Activation(old_active_id, new_active_id));
  }

  size_t InstallCount(web::WebStateID web_state_id) const {
    return std::ranges::count(installed_, web_state_id);
  }

  size_t UninstallCount(web::WebStateID web_state_id) const {
    return std::ranges::count(uninstalled_, web_state_id);
  }

  size_t DeletedCount(web::WebStateID web_state_id) const {
    return std::ranges::count(deleted_, web_state_id);
  }

  size_t ActivatedCount(Activation activation) const {
    return std::ranges::count(activated_, activation);
  }

  size_t ActivatedCount(web::WebStateID old_active_id,
                        web::WebStateID new_active_id) const {
    return ActivatedCount(Activation(old_active_id, new_active_id));
  }

 private:
  std::vector<web::WebStateID> installed_;
  std::vector<web::WebStateID> uninstalled_;
  std::vector<web::WebStateID> deleted_;
  std::vector<Activation> activated_;
};

}  // anonymous namespace

class TabsDependencyInstallerTest
    : public PlatformTest,
      public testing::WithParamInterface<TestParams> {
 public:
  TabsDependencyInstallerTest()
      : scoped_feature_list_helper_(std::get<FeatureState>(GetParam())) {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(
        profile_.get(), std::make_unique<FakeWebStateListDelegate>(
                            /* force_realization_on_activation */ true));
    web_state_list_ = browser_->GetWebStateList();
  }

  ~TabsDependencyInstallerTest() override { installer_.StopObserving(); }

 protected:
  web::WebTaskEnvironment task_environment_;

  ScopedFeatureListHelper scoped_feature_list_helper_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<Browser> browser_;
  raw_ptr<WebStateList> web_state_list_;
  TestTabsDependencyInstaller installer_;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    TabsDependencyInstallerTest,
    ::testing::Combine(
        ::testing::Values(FeatureState::kDisabled, FeatureState::kEnabled),
        ::testing::Values(
            TabsDependencyInstaller::Policy::kOnlyRealized,
            TabsDependencyInstaller::Policy::kAccordingToFeature)));

// Verifies that the appropriate inserted/removed methods are triggered
// when a WebState is inserted, replaced, or removed.
TEST_P(TabsDependencyInstallerTest, InsertReplaceAndRemoveWebState) {
  installer_.StartObserving(
      browser_.get(), std::get<TabsDependencyInstaller::Policy>(GetParam()));
  auto web_state_1 = std::make_unique<web::FakeWebState>();
  const web::WebStateID web_state_1_id = web_state_1->GetUniqueIdentifier();

  EXPECT_FALSE(installer_.WasInstalled(web_state_1_id));
  web_state_list_->InsertWebState(
      std::move(web_state_1),
      WebStateList::InsertionParams::Automatic().Activate());
  EXPECT_TRUE(installer_.WasInstalled(web_state_1_id));
  EXPECT_FALSE(installer_.WasUninstalled(web_state_1_id));

  auto web_state_2 = std::make_unique<web::FakeWebState>();
  const web::WebStateID web_state_2_id = web_state_2->GetUniqueIdentifier();
  web_state_list_->ReplaceWebStateAt(0, std::move(web_state_2));
  EXPECT_TRUE(installer_.WasUninstalled(web_state_1_id));
  EXPECT_TRUE(installer_.WasInstalled(web_state_2_id));
}

// Verifies that the appropriate inserted/removed methods are triggered
// for any WebStates that were already in the WebStateList prior to its
// observation.
TEST_P(TabsDependencyInstallerTest, RespectsPreexistingWebState) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web::WebStateID web_state_id = web_state->GetUniqueIdentifier();
  web_state_list_->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::Automatic().Activate());
  installer_.StartObserving(
      browser_.get(), std::get<TabsDependencyInstaller::Policy>(GetParam()));
  EXPECT_TRUE(installer_.WasInstalled(web_state_id));
}

// Verifies that the inserted/removed methods are not triggered for any
// unrealized WebStates.
TEST_P(TabsDependencyInstallerTest, UnrealizedWebStates) {
  installer_.StartObserving(
      browser_.get(), std::get<TabsDependencyInstaller::Policy>(GetParam()));
  auto web_state_1 = std::make_unique<web::FakeWebState>();
  web::WebStateID web_state_1_id = web_state_1->GetUniqueIdentifier();
  web_state_list_->InsertWebState(
      std::move(web_state_1),
      WebStateList::InsertionParams::Automatic().Activate());
  EXPECT_TRUE(installer_.WasInstalled(web_state_1_id));

  auto web_state_2 = std::make_unique<web::FakeWebState>();
  web_state_2->SetIsRealized(false);
  web::WebStateID web_state_2_id = web_state_2->GetUniqueIdentifier();

  const bool will_install_dependency_on_unrealized_web_states =
      (std::get<FeatureState>(GetParam()) == FeatureState::kDisabled) &&
      (std::get<TabsDependencyInstaller::Policy>(GetParam()) ==
       TabsDependencyInstaller::Policy::kAccordingToFeature);

  // Insert the unrealized webstate but don't have it activate (since that
  // forces realization).
  const int index_2 = web_state_list_->InsertWebState(std::move(web_state_2));
  if (!will_install_dependency_on_unrealized_web_states) {
    // The unrealized webstate should not have dependencies installed.
    EXPECT_FALSE(installer_.WasInstalled(web_state_2_id));
  } else {
    // The dependencies are installed even on unrealized WebStates if
    // the feature is disabled and the policy is kAccordingToFeature.
    EXPECT_TRUE(installer_.WasInstalled(web_state_2_id));
  }
  // Once realized, dependencies should be installed.
  ASSERT_NE(index_2, WebStateList::kInvalidIndex);
  web_state_list_->GetWebStateAt(index_2)->ForceRealized();
  EXPECT_TRUE(installer_.WasInstalled(web_state_2_id));

  auto web_state_3 = std::make_unique<web::FakeWebState>();
  web_state_3->SetIsRealized(false);
  web::WebStateID web_state_3_id = web_state_3->GetUniqueIdentifier();

  // Insert the unrealized webstate and activate it, forcing realization.
  web_state_list_->InsertWebState(
      std::move(web_state_3),
      WebStateList::InsertionParams::Automatic().Activate());
  // The formerly unrealized webstate should have dependencies installed.
  // (The webstate should also be realized, but that's not the responsibility
  // of the code under test).
  EXPECT_TRUE(installer_.WasInstalled(web_state_3_id));
  // Dependencies should have been installed only once
  EXPECT_EQ(1u, installer_.InstallCount(web_state_3_id));

  if (!will_install_dependency_on_unrealized_web_states) {
    auto web_state_4 = std::make_unique<web::FakeWebState>();
    web_state_4->SetIsRealized(false);
    web::WebStateID web_state_4_id = web_state_4->GetUniqueIdentifier();

    // Insert the unrealized webstate but don't have it activate (since that
    // forces realization).
    const int index_4 = web_state_list_->InsertWebState(std::move(web_state_4));
    // The unrealized webstate should not have dependencies installed.
    EXPECT_FALSE(installer_.WasInstalled(web_state_4_id));

    ASSERT_NE(index_4, WebStateList::kInvalidIndex);
    auto detached_web_state = web_state_list_->DetachWebStateAt(index_4);
    ASSERT_TRUE(detached_web_state);
    EXPECT_EQ(detached_web_state->GetUniqueIdentifier(), web_state_4_id);
    detached_web_state->ForceRealized();

    // The dependencies should not have been installed as the WebState
    // was realized after being removed from the WebStateList (and thus
    // no longer tracked by TabsDependencyInstaller).
    EXPECT_FALSE(installer_.WasInstalled(web_state_4_id));
  }
}

// Verifies that the installer is notified of permanent deletion of WebState.
TEST_P(TabsDependencyInstallerTest, Deleted) {
  struct TestCase {
    bool expect_notification;
    WebStateList::ClosingReason close_reason;
  };

  constexpr TestCase kTestCases[] = {
      {
          .expect_notification = false,
          .close_reason = WebStateList::ClosingReason::kDefault,
      },
      {
          .expect_notification = true,
          .close_reason = WebStateList::ClosingReason::kUserAction,
      },
      {
          .expect_notification = true,
          .close_reason = WebStateList::ClosingReason::kTabsCleanup,
      },
  };

  installer_.StartObserving(
      browser_.get(), std::get<TabsDependencyInstaller::Policy>(GetParam()));

  // Check that the method WebStateDeleted() is called if the tabs is closed
  // due to an user action or due to tabs cleanup.
  for (const TestCase& test_case : kTestCases) {
    auto web_state = std::make_unique<web::FakeWebState>();
    web_state->SetIsRealized(false);
    web::WebStateID web_state_id = web_state->GetUniqueIdentifier();
    const int index = web_state_list_->InsertWebState(std::move(web_state));
    ASSERT_NE(index, WebStateList::kInvalidIndex);

    web_state_list_->CloseWebStateAt(index, test_case.close_reason);
    EXPECT_EQ(installer_.WasDeleted(web_state_id),
              test_case.expect_notification);
  }

  // Check that the method WebStateDeleted() is not called if the tab is
  // detached.
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetIsRealized(false);
  web::WebStateID web_state_id = web_state->GetUniqueIdentifier();
  const int index = web_state_list_->InsertWebState(std::move(web_state));
  ASSERT_NE(index, WebStateList::kInvalidIndex);

  auto detached_web_state = web_state_list_->DetachWebStateAt(index);
  ASSERT_TRUE(detached_web_state);
  EXPECT_EQ(detached_web_state->GetUniqueIdentifier(), web_state_id);
  EXPECT_FALSE(installer_.WasDeleted(web_state_id));
}

// Verifies that the WebStateActivated() method is correctly called when
// the active WebState changes.
TEST_P(TabsDependencyInstallerTest, Activation) {
  installer_.StartObserving(
      browser_.get(), std::get<TabsDependencyInstaller::Policy>(GetParam()));

  auto web_state_1 = std::make_unique<web::FakeWebState>();
  web::WebStateID web_state_1_id = web_state_1->GetUniqueIdentifier();
  const int index_1 = web_state_list_->InsertWebState(
      std::move(web_state_1),
      WebStateList::InsertionParams::Automatic().Activate());
  ASSERT_NE(index_1, WebStateList::kInvalidIndex);

  // Check that the installer is notified when the first WebState is inserted.
  EXPECT_TRUE(installer_.WasActivated(web::WebStateID(), web_state_1_id));

  auto web_state_2 = std::make_unique<web::FakeWebState>();
  web::WebStateID web_state_2_id = web_state_2->GetUniqueIdentifier();
  const int index_2 = web_state_list_->InsertWebState(std::move(web_state_2));
  ASSERT_NE(index_2, WebStateList::kInvalidIndex);

  // Check that the installer is not notified when a WebState is inserted
  // without changing the active WebState.
  EXPECT_FALSE(installer_.WasActivated(web_state_1_id, web_state_2_id));

  // Check that activating a WebState causes the installer to be notified.
  web_state_list_->ActivateWebStateAt(index_2);
  EXPECT_TRUE(installer_.WasActivated(web_state_1_id, web_state_2_id));

  // Check that the installer is notified if the active WebState changes
  // when a WebState is detached/closed.
  auto detached_web_state = web_state_list_->DetachWebStateAt(index_2);
  ASSERT_TRUE(detached_web_state);
  EXPECT_EQ(detached_web_state->GetUniqueIdentifier(), web_state_2_id);
  EXPECT_TRUE(installer_.WasActivated(web_state_2_id, web_state_1_id));

  web_state_list_->CloseWebStateAt(index_1,
                                   WebStateList::ClosingReason::kUserAction);
  EXPECT_TRUE(installer_.WasActivated(web_state_1_id, web::WebStateID()));
}

// Verifies that no methods are triggered after stopping the observation.
TEST_P(TabsDependencyInstallerTest, Disconnect) {
  installer_.StartObserving(
      browser_.get(), std::get<TabsDependencyInstaller::Policy>(GetParam()));
  auto web_state_1 = std::make_unique<web::FakeWebState>();
  web::WebStateID web_state_1_id = web_state_1->GetUniqueIdentifier();

  web_state_list_->InsertWebState(
      std::move(web_state_1),
      WebStateList::InsertionParams::Automatic().Activate());
  EXPECT_TRUE(installer_.WasInstalled(web_state_1_id));
  installer_.StopObserving();

  EXPECT_TRUE(installer_.WasUninstalled(web_state_1_id));

  auto web_state_2 = std::make_unique<web::FakeWebState>();
  web::WebStateID web_state_2_id = web_state_2->GetUniqueIdentifier();

  web_state_list_->InsertWebState(
      std::move(web_state_2),
      WebStateList::InsertionParams::Automatic().Activate());
  EXPECT_FALSE(installer_.WasInstalled(web_state_2_id));
}

// Verifies that stopping the observation for an installer will uninstall it
// from the TabsDependencyInstallerManager.
TEST_P(TabsDependencyInstallerTest, UninstallForManagerAfterDisconnect) {
  TabsDependencyInstallerManager::CreateForBrowser(browser_.get());
  TabsDependencyInstallerManager* manager =
      TabsDependencyInstallerManager::FromBrowser(browser_.get());
  ASSERT_TRUE(manager);
  installer_.StartObserving(
      browser_.get(), std::get<TabsDependencyInstaller::Policy>(GetParam()));

  auto web_state = std::make_unique<web::FakeWebState>();
  const web::WebStateID web_state_id = web_state->GetUniqueIdentifier();

  manager->InstallDependencies(web_state.get());
  EXPECT_TRUE(installer_.WasInstalled(web_state_id));
  EXPECT_EQ(1u, installer_.InstallCount(web_state_id));
  EXPECT_FALSE(installer_.WasUninstalled(web_state_id));

  installer_.StopObserving();

  manager->UninstallDependencies(web_state.get());

  // The uninstall should not be performed since the test installer stops
  // observing before the manager starts uninstalling dependencies.
  EXPECT_FALSE(installer_.WasUninstalled(web_state_id));
  EXPECT_EQ(0u, installer_.UninstallCount(web_state_id));
}
