// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_picker/coordinator/tab_picker_mediator.h"

#import <optional>

#import "base/functional/callback_helpers.h"
#import "base/memory/raw_ptr.h"
#import "base/test/test_future.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/tab_picker_commands.h"
#import "ios/chrome/browser/snapshots/model/snapshot_browser_agent.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/tab_picker/public/tab_picker_snackbar_presenter.h"
#import "ios/chrome/browser/tab_picker/ui/tab_picker_consumer.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_collection_consumer.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/grid_item_identifier.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

class TabPickerMediatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    profile_ =
        profile_manager_.AddProfileWithBuilder(TestProfileIOS::Builder());
    browser_ = std::make_unique<TestBrowser>(profile_);
    SnapshotBrowserAgent::CreateForBrowser(browser_.get());
    SnapshotBrowserAgent::FromBrowser(browser_.get())
        ->SetSessionID("Identifier");

    mock_grid_consumer_ = OCMProtocolMock(@protocol(TabCollectionConsumer));
    mock_tab_picker_consumer_ = OCMProtocolMock(@protocol(TabPickerConsumer));
    mock_snackbar_presenter_ =
        OCMProtocolMock(@protocol(TabPickerSnackbarPresenter));

    params_ = [[TabPickerParams alloc]
        initWithSnackbarPresenter:mock_snackbar_presenter_];
    params_.maxTabAttachmentCount = 10;
  }

  void TearDown() override {
    browser_.reset();
    profile_ = nullptr;
    PlatformTest::TearDown();
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<TestProfileIOS> profile_ = nullptr;
  std::unique_ptr<TestBrowser> browser_;
  id mock_grid_consumer_;
  id mock_tab_picker_consumer_;
  id mock_snackbar_presenter_;
  TabPickerParams* params_;
};

// Tests that cancelling the tab picker invokes the completion block with
// nullopt.
TEST_F(TabPickerMediatorTest,
       TestCancelTabPicker_InvokesCompletionWithNullopt) {
  base::test::TestFuture<std::optional<TabPickerSelection>> future;
  TabPickerCompletionBlock completion =
      base::CallbackToBlock(future.GetRepeatingCallback());

  TabPickerMediator* mediator =
      [[TabPickerMediator alloc] initWithGridConsumer:mock_grid_consumer_
                                    tabPickerConsumer:mock_tab_picker_consumer_
                                               params:params_
                             tabPickerCompletionBlock:completion];
  mediator.browser = browser_.get();

  [mediator cancelTabPicker];
  ASSERT_TRUE(future.IsReady());
  EXPECT_FALSE(future.Get().has_value());

  // Subsequent call does not re-invoke.
  future.Clear();
  [mediator cancelTabPicker];
  EXPECT_FALSE(future.IsReady());

  [mediator disconnect];
}

// Tests that disconnecting without selection invokes the completion block with
// nullopt.
TEST_F(TabPickerMediatorTest, TestDisconnect_InvokesCompletionWithNullopt) {
  base::test::TestFuture<std::optional<TabPickerSelection>> future;
  TabPickerCompletionBlock completion =
      base::CallbackToBlock(future.GetCallback());

  TabPickerMediator* mediator =
      [[TabPickerMediator alloc] initWithGridConsumer:mock_grid_consumer_
                                    tabPickerConsumer:mock_tab_picker_consumer_
                                               params:params_
                             tabPickerCompletionBlock:completion];
  mediator.browser = browser_.get();

  [mediator disconnect];
  ASSERT_TRUE(future.IsReady());
  EXPECT_FALSE(future.Get().has_value());
}

// Tests that attaching tabs invokes completion with selection and
// suppresses cancellation on disconnect.
TEST_F(TabPickerMediatorTest,
       TestAttachSelectedTabs_InvokesCompletionWithSelection) {
  auto fake_web_state = std::make_unique<web::FakeWebState>();
  fake_web_state->SetVisibleURL(GURL("https://example.com"));
  web::WebStateID web_state_id = fake_web_state->GetUniqueIdentifier();
  SnapshotTabHelper::CreateForWebState(fake_web_state.get());
  browser_->GetWebStateList()->InsertWebState(std::move(fake_web_state));

  base::test::TestFuture<std::optional<TabPickerSelection>> future;
  TabPickerCompletionBlock completion =
      base::CallbackToBlock(future.GetRepeatingCallback());

  TabPickerMediator* mediator =
      [[TabPickerMediator alloc] initWithGridConsumer:mock_grid_consumer_
                                    tabPickerConsumer:mock_tab_picker_consumer_
                                               params:params_
                             tabPickerCompletionBlock:completion];
  mediator.browser = browser_.get();

  web::WebState* inserted_web_state =
      browser_->GetWebStateList()->GetWebStateAt(0);
  GridItemIdentifier* item =
      [GridItemIdentifier tabIdentifier:inserted_web_state];
  [mediator addToSelectionItemID:item];

  [mediator attachSelectedTabs];
  ASSERT_TRUE(future.IsReady());
  std::optional<TabPickerSelection> selection = future.Get();
  ASSERT_TRUE(selection.has_value());
  EXPECT_EQ(selection->selected_ids.count(web_state_id), 1u);

  future.Clear();
  [mediator disconnect];
  EXPECT_FALSE(future.IsReady());
}
