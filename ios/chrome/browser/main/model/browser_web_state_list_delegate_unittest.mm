// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/main/model/browser_web_state_list_delegate.h"

#import <tuple>

#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/tips_manager/model/tips_manager_ios_factory.h"
#import "ios/chrome/browser/voice/model/voice_search_navigations_tab_helper.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/common/features.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"

namespace {

// Forward-declaration of a tab helper. This is used as a way to check
// whether `AttachTabHelpers(...)` is called, but the tests themselves
// don't depend on a specific tab helper. Using an alias allow to change
// how the check is performed if in the future `AttachTabHelpers(...)`
// is modified.
using ExpectedTabHelper = VoiceSearchNavigationTabHelper;

// Represents state of the kCreateTabHelperOnlyForRealizedWebStates
// feature for a test case.
enum class FeatureState {
  kEnabled,
  kDisabled,
};

using BrowserWebStateListDelegateTestParam =
    std::tuple<BrowserWebStateListDelegate::InsertionPolicy,
               BrowserWebStateListDelegate::ActivationPolicy,
               FeatureState>;

// List all ContentWorlds. Necessary because calling SetWebFramesManager(...)
// with a kAllContentWorlds is not enough with FakeWebState.
constexpr web::ContentWorld kContentWorlds[] = {
    web::ContentWorld::kAllContentWorlds,
    web::ContentWorld::kPageContentWorld,
    web::ContentWorld::kIsolatedWorld,
};

// A class that initialize a ScopedFeatureList in its constructor.
class ScopedFeatureListWrapper {
 public:
  ScopedFeatureListWrapper(const base::Feature& feature, FeatureState state) {
    feature_list_.InitWithFeatureState(feature,
                                       state == FeatureState::kEnabled);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

}  // namespace

class BrowserWebStateListDelegateTest
    : public testing::TestWithParam<BrowserWebStateListDelegateTestParam> {
 public:
  BrowserWebStateListDelegateTest()
      : feature_list_(web::features::kCreateTabHelperOnlyForRealizedWebStates,
                      std::get<FeatureState>(GetParam())) {
    profile_ = TestProfileIOS::Builder().Build();
    profile_->CreateOffTheRecordProfileWithTestingFactories(
        {TestProfileIOS::TestingFactory{
            TipsManagerIOSFactory::GetInstance(),
            TipsManagerIOSFactory::GetDefaultFactory()}});
  }

  // Creates a fake WebState that is unrealized and off-the-record (this
  // reduces the number of tab helpers that are attached, avoiding those
  // that depends on KeyedService not created during testing).
  std::unique_ptr<web::WebState> CreateWebState() {
    auto web_state = std::make_unique<web::FakeWebState>();
    web_state->SetIsRealized(false);
    web_state->SetBrowserState(profile_->GetOffTheRecordProfile());

    for (const web::ContentWorld content_world : kContentWorlds) {
      web_state->SetWebFramesManager(
          content_world, std::make_unique<web::FakeWebFramesManager>());
    }
    return web_state;
  }

  ProfileIOS* profile() { return profile_.get(); }

 private:
  ScopedFeatureListWrapper feature_list_;
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
};

INSTANTIATE_TEST_SUITE_P(
    BrowserWebStateListDelegateTestWithPolicies,
    BrowserWebStateListDelegateTest,
    ::testing::Combine(
        ::testing::Values(
            BrowserWebStateListDelegate::InsertionPolicy::kDoNothing,
            BrowserWebStateListDelegate::InsertionPolicy::kAttachTabHelpers),
        ::testing::Values(
            BrowserWebStateListDelegate::ActivationPolicy::kDoNothing,
            BrowserWebStateListDelegate::ActivationPolicy::kForceRealization),
        ::testing::Values(FeatureState::kEnabled, FeatureState::kDisabled)));

// Tests that BrowserWebStateListDelegateTest respects the InsertionPolicy
// when a realized WebState is inserted.
TEST_P(BrowserWebStateListDelegateTest, InsertionPolicy) {
  const BrowserWebStateListDelegateTestParam param = GetParam();
  BrowserWebStateListDelegate delegate(
      profile()->GetOffTheRecordProfile(),
      std::get<BrowserWebStateListDelegate::InsertionPolicy>(param),
      std::get<BrowserWebStateListDelegate::ActivationPolicy>(param));

  std::unique_ptr<web::WebState> web_state = CreateWebState();
  web_state->ForceRealized();
  ASSERT_TRUE(web_state->IsRealized());
  ASSERT_FALSE(ExpectedTabHelper::FromWebState(web_state.get()));

  // Check that only the InsertionPolicy controls whether the tab helpers
  // are attached to a tab on insertion when the WebState is realized.
  delegate.WillAddWebState(web_state.get());
  switch (std::get<BrowserWebStateListDelegate::InsertionPolicy>(param)) {
    case BrowserWebStateListDelegate::InsertionPolicy::kDoNothing:
      EXPECT_TRUE(web_state->IsRealized());
      EXPECT_FALSE(ExpectedTabHelper::FromWebState(web_state.get()));
      break;

    case BrowserWebStateListDelegate::InsertionPolicy::kAttachTabHelpers:
      EXPECT_TRUE(web_state->IsRealized());
      EXPECT_TRUE(ExpectedTabHelper::FromWebState(web_state.get()));
      break;
  }
}

// Tests that BrowserWebStateListDelegateTest respects the InsertionPolicy and
// the kCreateTabHelperOnlyForRealizedWebStates feature when an unrealized
// WebState is inserted.
TEST_P(BrowserWebStateListDelegateTest, InsertionPolicy_UnrealizedWebState) {
  const BrowserWebStateListDelegateTestParam param = GetParam();
  BrowserWebStateListDelegate delegate(
      profile()->GetOffTheRecordProfile(),
      std::get<BrowserWebStateListDelegate::InsertionPolicy>(param),
      std::get<BrowserWebStateListDelegate::ActivationPolicy>(param));

  std::unique_ptr<web::WebState> web_state = CreateWebState();
  ASSERT_FALSE(web_state->IsRealized());
  ASSERT_FALSE(ExpectedTabHelper::FromWebState(web_state.get()));

  // Check that both kCreateTabHelperOnlyForRealizedWebStates feature
  // and InsertionPolicy controls whether the tab helpers are attached
  // to a tab on insertion when the WebState is unrealized.
  delegate.WillAddWebState(web_state.get());
  switch (std::get<BrowserWebStateListDelegate::InsertionPolicy>(param)) {
    case BrowserWebStateListDelegate::InsertionPolicy::kDoNothing:
      EXPECT_FALSE(web_state->IsRealized());
      EXPECT_FALSE(ExpectedTabHelper::FromWebState(web_state.get()));
      break;

    case BrowserWebStateListDelegate::InsertionPolicy::kAttachTabHelpers:
      EXPECT_FALSE(web_state->IsRealized());
      EXPECT_EQ(web::features::CreateTabHelperOnlyForRealizedWebStates(),
                ExpectedTabHelper::FromWebState(web_state.get()) == nullptr);
      break;
  }

  // If the kCreateTabHelperOnlyForRealizedWebStates is enabled, then the
  // tab helpers are only created when the WebState is realized, so check
  // that it is created on realization.
  web_state->ForceRealized();
  switch (std::get<BrowserWebStateListDelegate::InsertionPolicy>(param)) {
    case BrowserWebStateListDelegate::InsertionPolicy::kDoNothing:
      EXPECT_TRUE(web_state->IsRealized());
      EXPECT_FALSE(ExpectedTabHelper::FromWebState(web_state.get()));
      break;

    case BrowserWebStateListDelegate::InsertionPolicy::kAttachTabHelpers:
      EXPECT_TRUE(web_state->IsRealized());
      EXPECT_TRUE(ExpectedTabHelper::FromWebState(web_state.get()));
      break;
  }
}

// Tests that BrowserWebStateListDelegateTest respects the InsertionPolicy and
// the kCreateTabHelperOnlyForRealizedWebStates feature when an unrealized
// WebState is inserted and removed before being realized.
TEST_P(BrowserWebStateListDelegateTest, InsertionPolicy_RemovedBeforeRealized) {
  const BrowserWebStateListDelegateTestParam param = GetParam();
  BrowserWebStateListDelegate delegate(
      profile()->GetOffTheRecordProfile(),
      std::get<BrowserWebStateListDelegate::InsertionPolicy>(param),
      std::get<BrowserWebStateListDelegate::ActivationPolicy>(param));

  std::unique_ptr<web::WebState> web_state = CreateWebState();
  ASSERT_FALSE(web_state->IsRealized());
  ASSERT_FALSE(ExpectedTabHelper::FromWebState(web_state.get()));

  // Check that both kCreateTabHelperOnlyForRealizedWebStates feature
  // and InsertionPolicy controls whether the tab helpers are attached
  // to a tab on insertion when the WebState is unrealized.
  delegate.WillAddWebState(web_state.get());
  switch (std::get<BrowserWebStateListDelegate::InsertionPolicy>(param)) {
    case BrowserWebStateListDelegate::InsertionPolicy::kDoNothing:
      EXPECT_FALSE(web_state->IsRealized());
      EXPECT_FALSE(ExpectedTabHelper::FromWebState(web_state.get()));
      break;

    case BrowserWebStateListDelegate::InsertionPolicy::kAttachTabHelpers:
      EXPECT_FALSE(web_state->IsRealized());
      EXPECT_EQ(web::features::CreateTabHelperOnlyForRealizedWebStates(),
                ExpectedTabHelper::FromWebState(web_state.get()) == nullptr);
      break;
  }

  // If the kCreateTabHelperOnlyForRealizedWebStates is enabled, then the
  // tab helpers are only created when the WebState is realized, but they
  // must not be created if the WebState is removed before being realized.
  delegate.WillRemoveWebState(web_state.get());
  web_state->ForceRealized();

  switch (std::get<BrowserWebStateListDelegate::InsertionPolicy>(param)) {
    case BrowserWebStateListDelegate::InsertionPolicy::kDoNothing:
      EXPECT_TRUE(web_state->IsRealized());
      EXPECT_FALSE(ExpectedTabHelper::FromWebState(web_state.get()));
      break;

    case BrowserWebStateListDelegate::InsertionPolicy::kAttachTabHelpers:
      EXPECT_TRUE(web_state->IsRealized());
      EXPECT_EQ(web::features::CreateTabHelperOnlyForRealizedWebStates(),
                ExpectedTabHelper::FromWebState(web_state.get()) == nullptr);
      break;
  }
}

// Tests that BrowserWebStateListDelegateTest respects the ActivationPolicy
// when a WebState is marked as the active one.
TEST_P(BrowserWebStateListDelegateTest, ActivationPolicy) {
  const BrowserWebStateListDelegateTestParam param = GetParam();
  BrowserWebStateListDelegate delegate(
      profile()->GetOffTheRecordProfile(),
      std::get<BrowserWebStateListDelegate::InsertionPolicy>(param),
      std::get<BrowserWebStateListDelegate::ActivationPolicy>(param));

  std::unique_ptr<web::WebState> web_state = CreateWebState();
  ASSERT_FALSE(web_state->IsRealized());
  ASSERT_FALSE(ExpectedTabHelper::FromWebState(web_state.get()));

  // Check that only the ActivationPolicy controls whether the WebState is
  // forced to the realized state when marked as the active one.
  delegate.WillActivateWebState(web_state.get());
  switch (std::get<BrowserWebStateListDelegate::ActivationPolicy>(param)) {
    case BrowserWebStateListDelegate::ActivationPolicy::kDoNothing:
      EXPECT_FALSE(web_state->IsRealized());
      EXPECT_FALSE(ExpectedTabHelper::FromWebState(web_state.get()));
      break;

    case BrowserWebStateListDelegate::ActivationPolicy::kForceRealization:
      EXPECT_TRUE(web_state->IsRealized());
      EXPECT_FALSE(ExpectedTabHelper::FromWebState(web_state.get()));
      break;
  }
}
