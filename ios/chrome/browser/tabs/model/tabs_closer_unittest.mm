// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/model/tabs_closer.h"

#import <optional>

#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/scoped_observation.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/test_file_util.h"
#import "base/uuid.h"
#import "components/saved_tab_groups/public/saved_tab_group.h"
#import "components/saved_tab_groups/public/saved_tab_group_tab.h"
#import "components/saved_tab_groups/test_support/fake_tab_group_sync_service.h"
#import "components/sessions/core/tab_restore_service.h"
#import "components/tab_groups/tab_group_color.h"
#import "components/tab_groups/tab_group_id.h"
#import "components/tab_groups/tab_group_visual_data.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/sessions/model/fake_tab_restore_service.h"
#import "ios/chrome/browser/sessions/model/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service_factory.h"
#import "ios/chrome/browser/sessions/model/test_session_restoration_service.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/web_state_list_builder_from_description.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state_id.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "ui/base/page_transition_types.h"

using tab_groups::TabGroupVisualData;

namespace {

// Reentrancy observer to help checking the state of the tab closer during the
// operations.
class ReentrancyObserver : public WebStateListObserver {
 public:
  ReentrancyObserver(TabsCloser& tabs_closer) : tabs_closer_(&tabs_closer) {}

  void WebStateListDidChange(WebStateList* web_state_list,
                             const WebStateListChange& change,
                             const WebStateListStatus& status) final {
    can_undo_ = tabs_closer_->CanUndoCloseTabs();
  }

  bool CheckThatUndoCloseTabsWasNotPossible() {
    return can_undo_.has_value() && !can_undo_.value();
  }

 private:
  raw_ptr<TabsCloser> tabs_closer_;
  std::optional<bool> can_undo_ = std::nullopt;
};

// Controls whether the WebState is inserted as pinned or regular.
enum class InsertionPolicy {
  kPinned,
  kRegular,
};

// List all ContentWorlds. Necessary because calling SetWebFramesManager(...)
// with a kAllContentWorlds is not enough with FakeWebState.
constexpr web::ContentWorld kContentWorlds[] = {
    web::ContentWorld::kAllContentWorlds,
    web::ContentWorld::kPageContentWorld,
    web::ContentWorld::kIsolatedWorld,
};

// Session name used by the fake SceneState.
const char kSceneSessionID[] = "Identifier";

// WebStateListObserver checking whether a batch operation has been
// performed (i.e. started and then completed).
class ScopedTestWebStateListObserver final : public WebStateListObserver {
 public:
  ScopedTestWebStateListObserver() = default;

  // Start observing `web_state_list`.
  void Observe(WebStateList* web_state_list) {
    scoped_observation_.Observe(web_state_list);
  }

  // Returns whether a batch operation has been performed (i.e. started and then
  // completed).
  bool BatchOperationCompleted() const {
    return batch_operation_started_ && batch_operation_ended_;
  }

  // WebStateListObserver implementation.
  void WillBeginBatchOperation(WebStateList* web_state_list) final {
    batch_operation_started_ = true;
  }

  void BatchOperationEnded(WebStateList* web_state_list) final {
    batch_operation_ended_ = true;
  }

 private:
  // Records whether a batch operation started/ended.
  bool batch_operation_started_ = false;
  bool batch_operation_ended_ = false;

  // Scoped observation used to unregister itself when the object is destroyed.
  base::ScopedObservation<WebStateList, WebStateListObserver>
      scoped_observation_{this};
};

// Creates a FakeTabGroupSyncService.
std::unique_ptr<KeyedService> CreateFakeTabGroupSyncService(
    web::BrowserState* context) {
  return std::make_unique<tab_groups::FakeTabGroupSyncService>();
}

}  // namespace

class TabsCloserTest : public PlatformTest {
 public:
  TabsCloserTest() {
    // Create a TestProfileIOS with required services.
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        SessionRestorationServiceFactory::GetInstance(),
        TestSessionRestorationService::GetTestingFactory());
    builder.AddTestingFactory(IOSChromeTabRestoreServiceFactory::GetInstance(),
                              FakeTabRestoreService::GetTestingFactory());
    builder.AddTestingFactory(
        tab_groups::TabGroupSyncServiceFactory::GetInstance(),
        base::BindRepeating(&CreateFakeTabGroupSyncService));
    profile_ = std::move(builder).Build();

    fake_tab_group_service_ = static_cast<tab_groups::FakeTabGroupSyncService*>(
        tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile_.get()));

    // Initialize the AuthenticationService.
    AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
        profile_.get(), std::make_unique<FakeAuthenticationServiceDelegate>());

    scene_state_ = OCMClassMock([SceneState class]);
    OCMStub([scene_state_ sceneSessionID]).andReturn(@(kSceneSessionID));
    browser_ = Browser::Create(profile_.get(), scene_state_);
  }

  Browser* browser() { return browser_.get(); }

  sessions::TabRestoreService* restore_service() {
    return IOSChromeTabRestoreServiceFactory::GetForProfile(profile_.get());
  }

  tab_groups::FakeTabGroupSyncService* tab_group_service() {
    return fake_tab_group_service_;
  }

  // Appends a fake WebState in `browser_` using `policy` and `opener`.
  web::WebState* AppendWebState(InsertionPolicy policy, WebStateOpener opener) {
    const GURL url = GURL("https://example.com");
    auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
    navigation_manager->AddItem(url, ui::PAGE_TRANSITION_LINK);
    navigation_manager->SetLastCommittedItem(
        navigation_manager->GetItemAtIndex(0));

    auto web_state = std::make_unique<web::FakeWebState>();
    web_state->SetIsRealized(true);
    web_state->SetVisibleURL(url);
    web_state->SetBrowserState(browser_->GetProfile());
    web_state->SetNavigationManager(std::move(navigation_manager));
    web_state->SetNavigationItemCount(1);

    for (const web::ContentWorld content_world : kContentWorlds) {
      web_state->SetWebFramesManager(
          content_world, std::make_unique<web::FakeWebFramesManager>());
    }

    WebStateList* web_state_list = browser_->GetWebStateList();
    // Force the insertion at the end. Otherwise, the opener will trigger logic
    // to move an inserted WebState close to its opener.
    const WebStateList::InsertionParams params =
        WebStateList::InsertionParams::AtIndex(web_state_list->count())
            .Pinned(policy == InsertionPolicy::kPinned)
            .WithOpener(opener);
    const int insertion_index =
        web_state_list->InsertWebState(std::move(web_state), params);

    return web_state_list->GetWebStateAt(insertion_index);
  }

 private:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<ProfileIOS> profile_;
  __strong SceneState* scene_state_;
  std::unique_ptr<Browser> browser_;
  raw_ptr<tab_groups::FakeTabGroupSyncService> fake_tab_group_service_;
};

// Tests how a TabsCloser behaves when presented with a Browser containing
// no tabs.
//
// Variants: ClosePolicy::kAllTabs
TEST_F(TabsCloserTest, EmptyBrowser_ClosePolicyAllTabs) {
  TabsCloser tabs_closer(browser(), TabsCloser::ClosePolicy::kAllTabs);
  EXPECT_FALSE(tabs_closer.CanCloseTabs());
  EXPECT_FALSE(tabs_closer.CanUndoCloseTabs());
}

// Tests how a TabsCloser behaves when presented with a Browser containing
// no tabs.
//
// Variants: ClosePolicy::kRegularTabs
TEST_F(TabsCloserTest, EmptyBrowser_ClosePolicyRegularTabs) {
  TabsCloser tabs_closer(browser(), TabsCloser::ClosePolicy::kRegularTabs);
  EXPECT_FALSE(tabs_closer.CanCloseTabs());
  EXPECT_FALSE(tabs_closer.CanUndoCloseTabs());
}

// Tests how a TabsCloser behaves when presented with a Browser containing
// only regular tabs.
//
// Variants: ClosePolicy::kAllTabs
TEST_F(TabsCloserTest, BrowserWithOnlyRegularTabs_ClosePolicyAllTabs) {
  WebStateList* web_state_list = browser()->GetWebStateList();

  web::WebState* web_state0 =
      AppendWebState(InsertionPolicy::kRegular, WebStateOpener{});
  web::WebState* web_state1 =
      AppendWebState(InsertionPolicy::kRegular, WebStateOpener{});
  web::WebState* web_state2 =
      AppendWebState(InsertionPolicy::kRegular, WebStateOpener{web_state1, 0});

  ASSERT_EQ(web_state_list->count(), 3);
  ASSERT_EQ(web_state_list->GetWebStateAt(0), web_state0);
  ASSERT_EQ(web_state_list->GetWebStateAt(1), web_state1);
  ASSERT_EQ(web_state_list->GetWebStateAt(2), web_state2);

  TabsCloser tabs_closer(browser(), TabsCloser::ClosePolicy::kAllTabs);

  // Check that some tabs can be closed.
  EXPECT_TRUE(tabs_closer.CanCloseTabs());
  EXPECT_FALSE(tabs_closer.CanUndoCloseTabs());

  // Check that calling CloseTabs() closes all the tabs registered, allows to
  // undo the operation, and leaves the WebStateList empty.
  EXPECT_EQ(tabs_closer.CloseTabs(), 3);
  EXPECT_TRUE(tabs_closer.CanUndoCloseTabs());
  EXPECT_TRUE(web_state_list->empty());

  // Check that the TabRestoreService has not been informed of the close
  // operation yet (as it has not been confirmed).
  EXPECT_EQ(restore_service()->entries().size(), 0u);

  // Check that calling UndoCloseTabs() correctly restores all the closed tabs,
  // in the correct order, and with the opener-opened relationship intact.
  EXPECT_EQ(tabs_closer.UndoCloseTabs(), 3);
  ASSERT_EQ(web_state_list->count(), 3);
  EXPECT_EQ(web_state_list->pinned_tabs_count(), 0);
  EXPECT_EQ(web_state_list->GetWebStateAt(0), web_state0);
  EXPECT_EQ(web_state_list->GetWebStateAt(1), web_state1);
  EXPECT_EQ(web_state_list->GetWebStateAt(2), web_state2);
  EXPECT_EQ(web_state_list->GetOpenerOfWebStateAt(2).opener, web_state1);

  // Check that the TabRestoreService has not been informed of the close
  // operation yet (as the operation has been cancelled).
  EXPECT_EQ(restore_service()->entries().size(), 0u);

  // Check that calling CloseTabs() and ConfirmDeletion() correctly closes the
  // tabs and prevents undo.
  ASSERT_TRUE(tabs_closer.CanCloseTabs());
  EXPECT_EQ(tabs_closer.CloseTabs(), 3);
  EXPECT_EQ(tabs_closer.ConfirmDeletion(), 3);
  EXPECT_FALSE(tabs_closer.CanCloseTabs());
  EXPECT_FALSE(tabs_closer.CanUndoCloseTabs());

  // Check that the TabRestoreService has now been informed of the
  // close operation which has been confirmed.
  EXPECT_EQ(restore_service()->entries().size(), 3u);
}

// Tests how a TabsCloser behaves when presented with a Browser containing
// only regular tabs.
//
// Variants: ClosePolicy::kRegularTabs
TEST_F(TabsCloserTest, BrowserWithOnlyRegularTabs) {
  WebStateList* web_state_list = browser()->GetWebStateList();

  web::WebState* web_state0 =
      AppendWebState(InsertionPolicy::kRegular, WebStateOpener{});
  web::WebState* web_state1 =
      AppendWebState(InsertionPolicy::kRegular, WebStateOpener{});
  web::WebState* web_state2 =
      AppendWebState(InsertionPolicy::kRegular, WebStateOpener{web_state1, 0});

  ASSERT_EQ(web_state_list->count(), 3);
  ASSERT_EQ(web_state_list->GetWebStateAt(0), web_state0);
  ASSERT_EQ(web_state_list->GetWebStateAt(1), web_state1);
  ASSERT_EQ(web_state_list->GetWebStateAt(2), web_state2);

  TabsCloser tabs_closer(browser(), TabsCloser::ClosePolicy::kRegularTabs);

  // Check that some tabs can be closed.
  EXPECT_TRUE(tabs_closer.CanCloseTabs());
  EXPECT_FALSE(tabs_closer.CanUndoCloseTabs());

  // Check that calling CloseTabs() closes all the tabs registered, allows to
  // undo the operation, and leaves the WebStateList empty.
  EXPECT_EQ(tabs_closer.CloseTabs(), 3);
  EXPECT_TRUE(tabs_closer.CanUndoCloseTabs());
  EXPECT_TRUE(web_state_list->empty());

  // Check that the TabRestoreService has not been informed of the close
  // operation yet (as it has not been confirmed).
  EXPECT_EQ(restore_service()->entries().size(), 0u);

  // Check that calling UndoCloseTabs() correctly restores all the closed tabs,
  // in the correct order, and with the opener-opened relationship intact.
  EXPECT_EQ(tabs_closer.UndoCloseTabs(), 3);
  ASSERT_EQ(web_state_list->count(), 3);
  EXPECT_EQ(web_state_list->pinned_tabs_count(), 0);
  EXPECT_EQ(web_state_list->GetWebStateAt(0), web_state0);
  EXPECT_EQ(web_state_list->GetWebStateAt(1), web_state1);
  EXPECT_EQ(web_state_list->GetWebStateAt(2), web_state2);
  EXPECT_EQ(web_state_list->GetOpenerOfWebStateAt(2).opener, web_state1);

  // Check that the TabRestoreService has not been informed of the close
  // operation yet (as the operation has been cancelled).
  EXPECT_EQ(restore_service()->entries().size(), 0u);

  // Check that calling CloseTabs() and ConfirmDeletion() correctly closes the
  // tabs and prevents undo.
  ASSERT_TRUE(tabs_closer.CanCloseTabs());
  EXPECT_EQ(tabs_closer.CloseTabs(), 3);
  EXPECT_EQ(tabs_closer.ConfirmDeletion(), 3);
  EXPECT_FALSE(tabs_closer.CanCloseTabs());
  EXPECT_FALSE(tabs_closer.CanUndoCloseTabs());

  // Check that the TabRestoreService has now been informed of the
  // close operation which has been confirmed.
  EXPECT_EQ(restore_service()->entries().size(), 3u);
}

// Tests how a TabsCloser behaves when presented with a Browser containing
// only pinned tabs.
//
// Variants: ClosePolicy::kAllTabs
TEST_F(TabsCloserTest, BrowserWithOnlyPinnedTabs_ClosePolicyAllTabs) {
  WebStateList* web_state_list = browser()->GetWebStateList();

  web::WebState* web_state0 =
      AppendWebState(InsertionPolicy::kPinned, WebStateOpener{});
  web::WebState* web_state1 =
      AppendWebState(InsertionPolicy::kPinned, WebStateOpener{web_state0, 0});

  ASSERT_EQ(web_state_list->count(), 2);
  ASSERT_EQ(web_state_list->GetWebStateAt(0), web_state0);
  ASSERT_EQ(web_state_list->GetWebStateAt(1), web_state1);

  TabsCloser tabs_closer(browser(), TabsCloser::ClosePolicy::kAllTabs);

  // Check that some tabs can be closed.
  EXPECT_TRUE(tabs_closer.CanCloseTabs());
  EXPECT_FALSE(tabs_closer.CanUndoCloseTabs());

  // Check that calling CloseTabs() closes all the tabs registered, allows to
  // undo the operation, and leaves the WebStateList empty.
  EXPECT_EQ(tabs_closer.CloseTabs(), 2);
  EXPECT_TRUE(tabs_closer.CanUndoCloseTabs());
  EXPECT_TRUE(web_state_list->empty());

  // Check that the TabRestoreService has not been informed of the close
  // operation yet (as it has not been confirmed).
  EXPECT_EQ(restore_service()->entries().size(), 0u);

  // Check that calling UndoCloseTabs() correctly restores all the closed tabs,
  // in the correct order, and with the opener-opened relationship intact.
  EXPECT_EQ(tabs_closer.UndoCloseTabs(), 2);
  ASSERT_EQ(web_state_list->count(), 2);
  EXPECT_EQ(web_state_list->pinned_tabs_count(), 2);
  EXPECT_EQ(web_state_list->GetWebStateAt(0), web_state0);
  EXPECT_EQ(web_state_list->GetWebStateAt(1), web_state1);
  EXPECT_EQ(web_state_list->GetOpenerOfWebStateAt(1).opener, web_state0);

  // Check that the TabRestoreService has not been informed of the close
  // operation yet (as the operation has been cancelled).
  EXPECT_EQ(restore_service()->entries().size(), 0u);

  // Check that calling CloseTabs() and ConfirmDeletion() correctly closes the
  // tabs and prevents undo.
  ASSERT_TRUE(tabs_closer.CanCloseTabs());
  EXPECT_EQ(tabs_closer.CloseTabs(), 2);
  EXPECT_EQ(tabs_closer.ConfirmDeletion(), 2);
  EXPECT_FALSE(tabs_closer.CanCloseTabs());
  EXPECT_FALSE(tabs_closer.CanUndoCloseTabs());

  // Check that the TabRestoreService has now been informed of the
  // close operation which has been confirmed.
  EXPECT_EQ(restore_service()->entries().size(), 2u);
}

// Tests how a TabsCloser behaves when presented with a Browser containing
// only pinned tabs.
//
// Variants: ClosePolicy::kRegularTabs
TEST_F(TabsCloserTest, BrowserWithOnlyPinnedTabs_ClosePolicyRegularTabs) {
  WebStateList* web_state_list = browser()->GetWebStateList();

  web::WebState* web_state0 =
      AppendWebState(InsertionPolicy::kPinned, WebStateOpener{});
  web::WebState* web_state1 =
      AppendWebState(InsertionPolicy::kPinned, WebStateOpener{web_state0, 0});

  ASSERT_EQ(web_state_list->count(), 2);
  ASSERT_EQ(web_state_list->GetWebStateAt(0), web_state0);
  ASSERT_EQ(web_state_list->GetWebStateAt(1), web_state1);

  TabsCloser tabs_closer(browser(), TabsCloser::ClosePolicy::kRegularTabs);

  // Check that nothing can be closed since there are only pinned
  // tabs in the WebStateList.
  EXPECT_FALSE(tabs_closer.CanCloseTabs());
  EXPECT_FALSE(tabs_closer.CanUndoCloseTabs());
}

// Tests how a TabsCloser behaves when presented with a Browser containing
// regular and pinned tabs.
//
// Variants: ClosePolicy::kAllTabs
TEST_F(TabsCloserTest, BrowserWithRegularAndPinnedTabs_ClosePolicyAllTabs) {
  WebStateList* web_state_list = browser()->GetWebStateList();

  web::WebState* web_state0 =
      AppendWebState(InsertionPolicy::kPinned, WebStateOpener{});
  web::WebState* web_state1 =
      AppendWebState(InsertionPolicy::kPinned, WebStateOpener{web_state0, 0});
  web::WebState* web_state2 =
      AppendWebState(InsertionPolicy::kRegular, WebStateOpener{});
  web::WebState* web_state3 =
      AppendWebState(InsertionPolicy::kRegular, WebStateOpener{web_state1, 0});
  web::WebState* web_state4 =
      AppendWebState(InsertionPolicy::kRegular, WebStateOpener{web_state3, 0});

  ASSERT_EQ(web_state_list->count(), 5);
  ASSERT_EQ(web_state_list->GetWebStateAt(0), web_state0);
  ASSERT_EQ(web_state_list->GetWebStateAt(1), web_state1);
  ASSERT_EQ(web_state_list->GetWebStateAt(2), web_state2);
  ASSERT_EQ(web_state_list->GetWebStateAt(3), web_state3);
  ASSERT_EQ(web_state_list->GetWebStateAt(4), web_state4);

  TabsCloser tabs_closer(browser(), TabsCloser::ClosePolicy::kAllTabs);

  // Check that some tabs can be closed.
  EXPECT_TRUE(tabs_closer.CanCloseTabs());
  EXPECT_FALSE(tabs_closer.CanUndoCloseTabs());

  // Check that calling CloseTabs() closes all the tabs registered, allows to
  // undo the operation, and leaves the WebStateList empty.
  EXPECT_EQ(tabs_closer.CloseTabs(), 5);
  EXPECT_TRUE(tabs_closer.CanUndoCloseTabs());
  EXPECT_TRUE(web_state_list->empty());

  // Check that the TabRestoreService has not been informed of the close
  // operation yet (as it has not been confirmed).
  EXPECT_EQ(restore_service()->entries().size(), 0u);

  // Check that calling UndoCloseTabs() correctly restores all the closed tabs,
  // in the correct order, and with the opener-opened relationship intact.
  EXPECT_EQ(tabs_closer.UndoCloseTabs(), 5);
  ASSERT_EQ(web_state_list->count(), 5);
  EXPECT_EQ(web_state_list->pinned_tabs_count(), 2);
  EXPECT_EQ(web_state_list->GetWebStateAt(0), web_state0);
  EXPECT_EQ(web_state_list->GetWebStateAt(1), web_state1);
  EXPECT_EQ(web_state_list->GetWebStateAt(2), web_state2);
  EXPECT_EQ(web_state_list->GetWebStateAt(3), web_state3);
  EXPECT_EQ(web_state_list->GetWebStateAt(4), web_state4);
  EXPECT_EQ(web_state_list->GetOpenerOfWebStateAt(1).opener, web_state0);
  EXPECT_EQ(web_state_list->GetOpenerOfWebStateAt(3).opener, web_state1);
  EXPECT_EQ(web_state_list->GetOpenerOfWebStateAt(4).opener, web_state3);

  // Check that the TabRestoreService has not been informed of the close
  // operation yet (as the operation has been cancelled).
  EXPECT_EQ(restore_service()->entries().size(), 0u);

  // Check that calling CloseTabs() and ConfirmDeletion() correctly closes the
  // tabs and prevents undo.
  ASSERT_TRUE(tabs_closer.CanCloseTabs());
  EXPECT_EQ(tabs_closer.CloseTabs(), 5);
  EXPECT_EQ(tabs_closer.ConfirmDeletion(), 5);
  EXPECT_FALSE(tabs_closer.CanCloseTabs());
  EXPECT_FALSE(tabs_closer.CanUndoCloseTabs());

  // Check that the TabRestoreService has now been informed of the
  // close operation which has been confirmed.
  EXPECT_EQ(restore_service()->entries().size(), 5u);
}

// Tests how a TabsCloser behaves when presented with a Browser containing
// regular and pinned tabs.
//
// Variants: ClosePolicy::kRegularTabs
TEST_F(TabsCloserTest, BrowserWithRegularAndPinnedTabs_ClosePolicyRegularTabs) {
  WebStateList* web_state_list = browser()->GetWebStateList();

  web::WebState* web_state0 =
      AppendWebState(InsertionPolicy::kPinned, WebStateOpener{});
  web::WebState* web_state1 =
      AppendWebState(InsertionPolicy::kPinned, WebStateOpener{web_state0, 0});
  web::WebState* web_state2 =
      AppendWebState(InsertionPolicy::kRegular, WebStateOpener{});
  web::WebState* web_state3 =
      AppendWebState(InsertionPolicy::kRegular, WebStateOpener{web_state1, 0});
  web::WebState* web_state4 =
      AppendWebState(InsertionPolicy::kRegular, WebStateOpener{web_state3, 0});

  ASSERT_EQ(web_state_list->count(), 5);
  ASSERT_EQ(web_state_list->GetWebStateAt(0), web_state0);
  ASSERT_EQ(web_state_list->GetWebStateAt(1), web_state1);
  ASSERT_EQ(web_state_list->GetWebStateAt(2), web_state2);
  ASSERT_EQ(web_state_list->GetWebStateAt(3), web_state3);
  ASSERT_EQ(web_state_list->GetWebStateAt(4), web_state4);

  TabsCloser tabs_closer(browser(), TabsCloser::ClosePolicy::kRegularTabs);

  // Check that some tabs can be closed.
  EXPECT_TRUE(tabs_closer.CanCloseTabs());
  EXPECT_FALSE(tabs_closer.CanUndoCloseTabs());

  // Check that calling CloseTabs() closes only the regular tabs,
  // allow to undo the operation and leave the WebStateList with
  // only pinned tabs.
  EXPECT_EQ(tabs_closer.CloseTabs(), 3);
  EXPECT_TRUE(tabs_closer.CanUndoCloseTabs());
  EXPECT_EQ(web_state_list->count(), 2);
  EXPECT_EQ(web_state_list->pinned_tabs_count(), 2);

  // Check that the TabRestoreService has not been informed of the close
  // operation yet (as it has not been confirmed).
  EXPECT_EQ(restore_service()->entries().size(), 0u);

  // Check that calling UndoCloseTabs() correctly restores all the closed tabs,
  // in the correct order, and with the opener-opened relationship intact.
  EXPECT_EQ(tabs_closer.UndoCloseTabs(), 3);
  ASSERT_EQ(web_state_list->count(), 5);
  EXPECT_EQ(web_state_list->pinned_tabs_count(), 2);
  EXPECT_EQ(web_state_list->GetWebStateAt(0), web_state0);
  EXPECT_EQ(web_state_list->GetWebStateAt(1), web_state1);
  EXPECT_EQ(web_state_list->GetWebStateAt(2), web_state2);
  EXPECT_EQ(web_state_list->GetWebStateAt(3), web_state3);
  EXPECT_EQ(web_state_list->GetWebStateAt(4), web_state4);
  EXPECT_EQ(web_state_list->GetOpenerOfWebStateAt(1).opener, web_state0);
  EXPECT_EQ(web_state_list->GetOpenerOfWebStateAt(3).opener, web_state1);
  EXPECT_EQ(web_state_list->GetOpenerOfWebStateAt(4).opener, web_state3);

  // Check that the TabRestoreService has not been informed of the close
  // operation yet (as the operation has been cancelled).
  EXPECT_EQ(restore_service()->entries().size(), 0u);

  // Check that calling CloseTabs() and ConfirmDeletion() correctly closes the
  // tabs and prevents undo.
  ASSERT_TRUE(tabs_closer.CanCloseTabs());
  EXPECT_EQ(tabs_closer.CloseTabs(), 3);
  EXPECT_EQ(tabs_closer.ConfirmDeletion(), 3);
  EXPECT_FALSE(tabs_closer.CanCloseTabs());
  EXPECT_FALSE(tabs_closer.CanUndoCloseTabs());

  // Check that the TabRestoreService has now been informed of the
  // close operation which has been confirmed.
  EXPECT_EQ(restore_service()->entries().size(), 3u);
}

// Tests that TabsCloser reinstates the groups when undoing.
//
// Variants: ClosePolicy::kAllTabs
TEST_F(TabsCloserTest, GroupedTabs_ClosePolicyAllTabs) {
  WebStateList* web_state_list = browser()->GetWebStateList();
  WebStateListBuilderFromDescription builder(web_state_list);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription(
      "a b | c [ 0 d e ] f [ 1 g h i ] j", browser()->GetProfile()));
  // Store the initial groups visual data to compare after Undo.
  const tab_groups::TabGroupVisualData visual_data_0 =
      builder.GetTabGroupForIdentifier('0')->visual_data();
  const tab_groups::TabGroupVisualData visual_data_1 =
      builder.GetTabGroupForIdentifier('1')->visual_data();
  // Store the initial WebStates to compare after Undo.
  std::vector<web::WebState*> initial_web_states;
  for (int i = 0; i < web_state_list->count(); ++i) {
    initial_web_states.push_back(web_state_list->GetWebStateAt(i));
  }

  TabsCloser tabs_closer(browser(), TabsCloser::ClosePolicy::kAllTabs);

  // Check that some tabs can be closed.
  EXPECT_TRUE(tabs_closer.CanCloseTabs());
  EXPECT_FALSE(tabs_closer.CanUndoCloseTabs());

  // Check that calling CloseTabs() closes all the tabs registered, allows to
  // undo the operation, and leaves the WebStateList empty.
  EXPECT_EQ(tabs_closer.CloseTabs(), 10);
  EXPECT_FALSE(tabs_closer.CanCloseTabs());
  EXPECT_TRUE(tabs_closer.CanUndoCloseTabs());
  EXPECT_EQ("|", builder.GetWebStateListDescription());

  // Check that the TabRestoreService has not been informed of the close
  // operation yet (as it has not been confirmed).
  EXPECT_EQ(restore_service()->entries().size(), 0u);

  // Check that calling UndoCloseTabs() correctly restores all the closed tabs,
  // in the correct order, and in the correct groups (note that the identifiers
  // have been lost but the structure is the same).
  EXPECT_EQ(tabs_closer.UndoCloseTabs(), 10);
  EXPECT_EQ("_ _ | _ [ _ _ _ ] _ [ _ _ _ _ ] _",
            builder.GetWebStateListDescription());
  // Compare the group's visual data with the initial ones.
  const TabGroup* actual_group_0 = web_state_list->GetGroupOfWebStateAt(3);
  EXPECT_EQ(visual_data_0, actual_group_0->visual_data());
  const TabGroup* actual_group_1 = web_state_list->GetGroupOfWebStateAt(6);
  EXPECT_EQ(visual_data_1, actual_group_1->visual_data());
  // Compare the WebStates with the initial ones.
  for (int i = 0; i < web_state_list->count(); ++i) {
    EXPECT_EQ(initial_web_states[i], web_state_list->GetWebStateAt(i));
  }

  // Check that the TabRestoreService has not been informed of the close
  // operation yet (as the operation has been cancelled).
  EXPECT_EQ(restore_service()->entries().size(), 0u);

  // Check that calling CloseTabs() and ConfirmDeletion() correctly closes the
  // tabs and prevents undo.
  ASSERT_TRUE(tabs_closer.CanCloseTabs());
  EXPECT_EQ(tabs_closer.CloseTabs(), 10);
  EXPECT_EQ(tabs_closer.ConfirmDeletion(), 10);
  EXPECT_FALSE(tabs_closer.CanCloseTabs());
  EXPECT_FALSE(tabs_closer.CanUndoCloseTabs());
  EXPECT_EQ("|", builder.GetWebStateListDescription());

  // Check that the TabRestoreService has now been informed of the
  // close operation which has been confirmed.
  EXPECT_EQ(restore_service()->entries().size(), 10u);
}

// Tests that TabsCloser reinstates the groups when undoing.
//
// Variants: ClosePolicy::kRegularTabs
TEST_F(TabsCloserTest, GroupedTabs_ClosePolicyRegularTabs) {
  WebStateList* web_state_list = browser()->GetWebStateList();
  WebStateListBuilderFromDescription builder(web_state_list);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription(
      "a b | c [ 0 d e ] f [ 1 g h i ] j", browser()->GetProfile()));
  // Store the initial groups visual data to compare after Undo.
  const tab_groups::TabGroupVisualData visual_data_0 =
      builder.GetTabGroupForIdentifier('0')->visual_data();
  const tab_groups::TabGroupVisualData visual_data_1 =
      builder.GetTabGroupForIdentifier('1')->visual_data();
  // Store the initial WebStates to compare after Undo.
  std::vector<web::WebState*> initial_web_states;
  for (int i = 0; i < web_state_list->count(); ++i) {
    initial_web_states.push_back(web_state_list->GetWebStateAt(i));
  }

  TabsCloser tabs_closer(browser(), TabsCloser::ClosePolicy::kRegularTabs);

  // Check that some tabs can be closed.
  EXPECT_TRUE(tabs_closer.CanCloseTabs());
  EXPECT_FALSE(tabs_closer.CanUndoCloseTabs());

  // Check that calling CloseTabs() closes all the tabs registered, allows to
  // undo the operation, and leaves the WebStateList with only pinned tabs.
  EXPECT_EQ(tabs_closer.CloseTabs(), 8);
  EXPECT_FALSE(tabs_closer.CanCloseTabs());
  EXPECT_TRUE(tabs_closer.CanUndoCloseTabs());
  EXPECT_EQ("a b |", builder.GetWebStateListDescription());

  // Check that the TabRestoreService has not been informed of the close
  // operation yet (as it has not been confirmed).
  EXPECT_EQ(restore_service()->entries().size(), 0u);

  // Check that calling UndoCloseTabs() correctly restores all the closed tabs,
  // in the correct order, and in the correct groups (note that the identifiers
  // have been lost but the structure is the same).
  EXPECT_EQ(tabs_closer.UndoCloseTabs(), 8);
  EXPECT_EQ("a b | _ [ _ _ _ ] _ [ _ _ _ _ ] _",
            builder.GetWebStateListDescription());
  // Compare the group's visual data with the initial ones.
  const TabGroup* actual_group_0 = web_state_list->GetGroupOfWebStateAt(3);
  EXPECT_EQ(visual_data_0, actual_group_0->visual_data());
  const TabGroup* actual_group_1 = web_state_list->GetGroupOfWebStateAt(6);
  EXPECT_EQ(visual_data_1, actual_group_1->visual_data());
  // Compare the WebStates with the initial ones.
  for (int i = 0; i < web_state_list->count(); ++i) {
    EXPECT_EQ(initial_web_states[i], web_state_list->GetWebStateAt(i));
  }

  // Check that the TabRestoreService has not been informed of the close
  // operation yet (as the operation has been cancelled).
  EXPECT_EQ(restore_service()->entries().size(), 0u);

  // Check that calling CloseTabs() and ConfirmDeletion() correctly closes the
  // tabs and prevents undo.
  ASSERT_TRUE(tabs_closer.CanCloseTabs());
  EXPECT_EQ(tabs_closer.CloseTabs(), 8);
  EXPECT_EQ(tabs_closer.ConfirmDeletion(), 8);
  EXPECT_FALSE(tabs_closer.CanCloseTabs());
  EXPECT_FALSE(tabs_closer.CanUndoCloseTabs());
  EXPECT_EQ("a b |", builder.GetWebStateListDescription());

  // Check that the TabRestoreService has now been informed of the
  // close operation which has been confirmed.
  EXPECT_EQ(restore_service()->entries().size(), 8u);
}

// Check that TabsCloser returns that there it is not possible to undo a close
// operation while undo is in progress.
TEST_F(TabsCloserTest, UndoCloseTabs_Reentrancy) {
  WebStateList* web_state_list = browser()->GetWebStateList();
  WebStateListBuilderFromDescription builder(web_state_list);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription(
      "a b | c d e", browser()->GetProfile()));

  TabsCloser tabs_closer(browser(), TabsCloser::ClosePolicy::kAllTabs);

  EXPECT_EQ(tabs_closer.CloseTabs(), 5);

  ReentrancyObserver observer(tabs_closer);
  base::ScopedObservation<WebStateList, WebStateListObserver> scoped_observer(
      &observer);
  scoped_observer.Observe(web_state_list);

  tabs_closer.UndoCloseTabs();
  EXPECT_TRUE(observer.CheckThatUndoCloseTabsWasNotPossible());
}

// Checks that close all/undo is correctly updating the TabGroupSyncService,
// both when it hasn't been modified and when it has been modified.
TEST_F(TabsCloserTest, UndoCloseTabs_SavedTabs) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {kTabGroupsIPad, kModernTabStrip, kTabGroupSync}, {});

  WebStateList* web_state_list = browser()->GetWebStateList();
  WebStateListBuilderFromDescription builder(web_state_list);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription(
      "| a [ 0 b ] c d [ 1 e ]", browser()->GetProfile()));

  // Add the two groups.
  tab_groups::FakeTabGroupSyncService* service = tab_group_service();
  tab_groups::TabGroupId first_local_id =
      web_state_list->GetGroupOfWebStateAt(1)->tab_group_id();
  web::WebStateID first_tab_id =
      web_state_list->GetWebStateAt(1)->GetUniqueIdentifier();
  base::Uuid first_group_id = base::Uuid::GenerateRandomV4();
  tab_groups::SavedTabGroupTab first_tab(
      GURL("http://first-tab.com"), u"first tab", first_group_id,
      std::make_optional(0), base::Uuid::GenerateRandomV4(),
      first_tab_id.identifier());
  tab_groups::SavedTabGroup first_saved_group(
      u"first title", tab_groups::TabGroupColorId::kBlue, {first_tab},
      std::make_optional(0), first_group_id, first_local_id);
  service->AddGroup(first_saved_group);

  tab_groups::TabGroupId second_local_id =
      web_state_list->GetGroupOfWebStateAt(4)->tab_group_id();
  base::Uuid second_group_id = base::Uuid::GenerateRandomV4();
  web::WebStateID second_tab_id =
      web_state_list->GetWebStateAt(4)->GetUniqueIdentifier();
  tab_groups::SavedTabGroupTab second_tab(
      GURL("http://second-tab.com"), u"second tab", second_group_id,
      std::make_optional(0), base::Uuid::GenerateRandomV4(),
      second_tab_id.identifier());
  tab_groups::SavedTabGroup second_saved_group(
      u"second title", tab_groups::TabGroupColorId::kBlue, {second_tab},
      std::make_optional(0), second_group_id, second_local_id);
  service->AddGroup(second_saved_group);

  TabsCloser tabs_closer(browser(), TabsCloser::ClosePolicy::kRegularTabs);

  // First check: no modification between Close All and Undo.

  // Close all.
  EXPECT_EQ(tabs_closer.CloseTabs(), 5);

  // Check that the two groups are still in the service, with no local group /
  // tab.
  EXPECT_TRUE(service->GetGroup(first_group_id));
  EXPECT_FALSE(service->GetGroup(first_group_id)->local_group_id().has_value());
  EXPECT_FALSE(service->GetGroup(first_group_id)
                   ->saved_tabs()[0]
                   .local_tab_id()
                   .has_value());
  EXPECT_TRUE(service->GetGroup(second_group_id));
  EXPECT_FALSE(
      service->GetGroup(second_group_id)->local_group_id().has_value());
  EXPECT_FALSE(service->GetGroup(second_group_id)
                   ->saved_tabs()[0]
                   .local_tab_id()
                   .has_value());

  // Undo.
  tabs_closer.UndoCloseTabs();

  // Check that the group are re-associated.
  EXPECT_EQ(2ul, web_state_list->GetGroups().size());
  EXPECT_EQ(5, web_state_list->count());
  EXPECT_TRUE(service->GetGroup(first_group_id));
  EXPECT_EQ(first_local_id,
            service->GetGroup(first_group_id)->local_group_id().value());
  EXPECT_TRUE(service->GetGroup(second_group_id));
  EXPECT_EQ(second_local_id,
            service->GetGroup(second_group_id)->local_group_id().value());

  // Second check: a group has been removed between Close All and Undo.

  // Close all.
  EXPECT_EQ(tabs_closer.CloseTabs(), 5);

  // Check that the two groups are still in the service, with no local group /
  // tab.
  EXPECT_TRUE(service->GetGroup(first_group_id));
  EXPECT_FALSE(service->GetGroup(first_group_id)->local_group_id().has_value());
  EXPECT_TRUE(service->GetGroup(second_group_id));
  EXPECT_FALSE(
      service->GetGroup(second_group_id)->local_group_id().has_value());

  // Remove a group from a sync update.
  service->RemoveGroup(first_group_id);

  // Undo.
  tabs_closer.UndoCloseTabs();

  // Check that the first group deleted (but not its tabs) and the second is
  // re-associated.
  EXPECT_EQ(1ul, web_state_list->GetGroups().size());
  EXPECT_EQ(5, web_state_list->count());
  EXPECT_FALSE(service->GetGroup(first_group_id));
  EXPECT_EQ(second_local_id,
            web_state_list->GetGroupOfWebStateAt(4)->tab_group_id());
  EXPECT_TRUE(service->GetGroup(second_group_id));
  EXPECT_EQ(second_local_id,
            service->GetGroup(second_group_id)->local_group_id().value());
}
