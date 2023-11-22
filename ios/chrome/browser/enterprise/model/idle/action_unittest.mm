// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/model/idle/action.h"

#import "base/test/gmock_callback_support.h"
#import "base/test/mock_callback.h"
#import "base/test/scoped_feature_list.h"
#import "base/time/time.h"
#import "components/enterprise/idle/idle_features.h"
#import "components/enterprise/idle/idle_pref_names.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/browsing_data/model/fake_browsing_data_remover.h"
#import "ios/chrome/browser/enterprise/model/idle/action_runner_impl.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using ::testing::_;

namespace enterprise_idle {

class IdleActionTest : public PlatformTest {
 protected:
  using ActionQueue = ActionFactory::ActionQueue;
  void SetUp() override {
    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
    scoped_feature_list_->InitWithFeatures({enterprise_idle::kIdleTimeout}, {});
    TestChromeBrowserState::Builder test_cbs_builder;
    browser_state_ = test_cbs_builder.Build();
    main_browsing_data_remover_ = std::make_unique<FakeBrowsingDataRemover>();
    incognito_browsing_data_remover_ =
        std::make_unique<FakeBrowsingDataRemover>();
    action_factory_ = std::make_unique<ActionFactory>();
  }

  void TearDown() override {
    task_environment_.RunUntilIdle();
    main_browsing_data_remover_.reset();
    incognito_browsing_data_remover_.reset();
    browser_state_.reset();
  }

  ActionFactory::ActionQueue GetActions(std::vector<ActionType> action_types) {
    return action_factory_->Build(action_types, main_remover(),
                                  incognito_remover());
  }

  TestChromeBrowserState* browser_state() { return browser_state_.get(); }

  FakeBrowsingDataRemover* main_remover() {
    return main_browsing_data_remover_.get();
  }

  FakeBrowsingDataRemover* incognito_remover() {
    return incognito_browsing_data_remover_.get();
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<ActionFactory> action_factory_;
  std::unique_ptr<FakeBrowsingDataRemover> main_browsing_data_remover_;
  std::unique_ptr<FakeBrowsingDataRemover> incognito_browsing_data_remover_;
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
};

TEST_F(IdleActionTest, ClearBrowsingHistory) {
  ActionQueue actions = GetActions({ActionType::kClearBrowsingHistory});
  base::MockCallback<Action::Continuation> continuation;
  actions.top()->Run(browser_state(), continuation.Get());
  EXPECT_EQ(BrowsingDataRemoveMask::REMOVE_HISTORY,
            main_remover()->GetLastUsedRemovalMask());
  EXPECT_EQ(BrowsingDataRemoveMask::REMOVE_HISTORY,
            incognito_remover()->GetLastUsedRemovalMask());
  actions.pop();
}

TEST_F(IdleActionTest, ClearCookies) {
  ActionQueue actions = GetActions({ActionType::kClearCookiesAndOtherSiteData});
  base::MockCallback<Action::Continuation> continuation;
  actions.top()->Run(browser_state(), continuation.Get());
  EXPECT_EQ(BrowsingDataRemoveMask::REMOVE_SITE_DATA,
            main_remover()->GetLastUsedRemovalMask());
  EXPECT_EQ(BrowsingDataRemoveMask::REMOVE_SITE_DATA,
            incognito_remover()->GetLastUsedRemovalMask());
  actions.pop();
}

TEST_F(IdleActionTest, ClearCache) {
  ActionQueue actions = GetActions({ActionType::kClearCachedImagesAndFiles});
  base::MockCallback<Action::Continuation> continuation;
  actions.top()->Run(browser_state(), continuation.Get());
  EXPECT_EQ(BrowsingDataRemoveMask::REMOVE_CACHE,
            main_remover()->GetLastUsedRemovalMask());
  EXPECT_EQ(BrowsingDataRemoveMask::REMOVE_CACHE,
            incognito_remover()->GetLastUsedRemovalMask());
  actions.pop();
}

TEST_F(IdleActionTest, ClearPasswordSignin) {
  ActionQueue actions = GetActions({ActionType::kClearPasswordSignin});
  base::MockCallback<Action::Continuation> continuation;
  actions.top()->Run(browser_state(), continuation.Get());
  EXPECT_EQ(BrowsingDataRemoveMask::REMOVE_PASSWORDS,
            main_remover()->GetLastUsedRemovalMask());
  EXPECT_EQ(BrowsingDataRemoveMask::REMOVE_PASSWORDS,
            incognito_remover()->GetLastUsedRemovalMask());
  actions.pop();
}

TEST_F(IdleActionTest, ClearAutofill) {
  ActionQueue actions = GetActions({ActionType::kClearAutofill});
  base::MockCallback<Action::Continuation> continuation;
  actions.top()->Run(browser_state(), continuation.Get());
  EXPECT_EQ(BrowsingDataRemoveMask::REMOVE_FORM_DATA,
            main_remover()->GetLastUsedRemovalMask());
  actions.top()->Run(browser_state(), continuation.Get());
  EXPECT_EQ(BrowsingDataRemoveMask::REMOVE_FORM_DATA,
            incognito_remover()->GetLastUsedRemovalMask());
  actions.pop();
}

TEST_F(IdleActionTest, MultipleTypesAndSuccess) {
  ActionQueue actions = GetActions(
      {ActionType::kClearBrowsingHistory, ActionType::kClearAutofill});
  base::MockCallback<Action::Continuation> continuation;
  actions.top()->Run(browser_state(), continuation.Get());
  EXPECT_EQ(BrowsingDataRemoveMask::REMOVE_HISTORY |
                BrowsingDataRemoveMask::REMOVE_FORM_DATA,
            main_remover()->GetLastUsedRemovalMask());
  EXPECT_EQ(BrowsingDataRemoveMask::REMOVE_HISTORY |
                BrowsingDataRemoveMask::REMOVE_FORM_DATA,
            incognito_remover()->GetLastUsedRemovalMask());
  actions.pop();
}

TEST_F(IdleActionTest, MultipleTypesAndFailure) {
  main_remover()->SetFailedForTesting();
  ActionQueue actions = GetActions(
      {ActionType::kClearBrowsingHistory, ActionType::kClearAutofill});
  ASSERT_EQ(1u, actions.size());
  EXPECT_EQ(static_cast<int>(ActionType::kClearBrowsingHistory),
            actions.top()->priority());

  // The callback should run with success=false.
  base::MockCallback<Action::Continuation> continuation;
  base::RunLoop run_loop;
  EXPECT_CALL(continuation, Run(/*success=*/false))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  actions.top()->Run(browser_state(), continuation.Get());
  run_loop.Run();
  actions.pop();
}

TEST_F(IdleActionTest, SignOut) {
  ActionQueue actions = GetActions({ActionType::kSignOut});
  // Check that the right action is added.
  EXPECT_EQ(static_cast<int>(ActionType::kSignOut), actions.top()->priority());
  base::MockCallback<Action::Continuation> continuation;
  actions.top()->Run(browser_state(), continuation.Get());
  actions.pop();
}

TEST_F(IdleActionTest, CloseTabs) {
  ActionQueue actions = GetActions({ActionType::kCloseTabs});
  base::MockCallback<Action::Continuation> continuation;
  // Check that the right action is added.
  EXPECT_EQ(static_cast<int>(ActionType::kCloseTabs),
            actions.top()->priority());
  actions.top()->Run(browser_state(), continuation.Get());
  actions.pop();
}

}  // namespace enterprise_idle
