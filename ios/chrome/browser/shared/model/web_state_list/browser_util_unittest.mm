// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/web_state_list/browser_util.h"

#import <memory>

#import "base/memory/raw_ptr.h"
#import "components/tab_groups/tab_group_id.h"
#import "ios/chrome/browser/sessions/model/fake_tab_restore_service.h"
#import "ios/chrome/browser/sessions/model/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/snapshots/model/snapshot_browser_agent.h"
#import "ios/chrome/browser/snapshots/model/snapshot_id.h"
#import "ios/chrome/browser/snapshots/model/snapshot_storage_wrapper.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/test/ios/ui_image_test_utils.h"

using ui::test::uiimage_utils::UIImagesAreEqual;
using ui::test::uiimage_utils::UIImageWithSizeAndSolidColor;

namespace {

// Name of the directory where snapshots are saved.
const char kIdentifier0[] = "Identifier0";
const char kIdentifier1[] = "Identifier1";

}  // namespace

// Test fixture for testing functions in browser_util.h/mm.
class BrowserUtilTest : public PlatformTest {
 protected:
  BrowserUtilTest() {
    TestProfileIOS::Builder test_profile_builder;
    test_profile_builder.AddTestingFactory(
        IOSChromeTabRestoreServiceFactory::GetInstance(),
        FakeTabRestoreService::GetTestingFactory());

    profile_ = std::move(test_profile_builder).Build();

    browser_ = std::make_unique<TestBrowser>(profile_.get());
    other_browser_ = std::make_unique<TestBrowser>(profile_.get());
    incognito_browser_ =
        std::make_unique<TestBrowser>(profile_->GetOffTheRecordProfile());
    other_incognito_browser_ =
        std::make_unique<TestBrowser>(profile_->GetOffTheRecordProfile());

    browser_list_ = BrowserListFactory::GetForProfile(profile_.get());
    browser_list_->AddBrowser(browser_.get());
    browser_list_->AddBrowser(other_browser_.get());
    browser_list_->AddBrowser(incognito_browser_.get());
    browser_list_->AddBrowser(other_incognito_browser_.get());

    SnapshotBrowserAgent::CreateForBrowser(browser_.get());
    SnapshotBrowserAgent::CreateForBrowser(other_browser_.get());
    SnapshotBrowserAgent::CreateForBrowser(incognito_browser_.get());
    SnapshotBrowserAgent::CreateForBrowser(other_incognito_browser_.get());

    AppendNewWebState(browser_.get());
    AppendNewWebState(browser_.get());
    AppendNewWebState(browser_.get());
    AppendNewWebState(incognito_browser_.get());

    tab_restore_service_ =
        IOSChromeTabRestoreServiceFactory::GetForProfile(profile_.get());
  }

  // Appends a new web state in the web state list of `browser`.
  web::FakeWebState* AppendNewWebState(Browser* browser) {
    auto fake_web_state = std::make_unique<web::FakeWebState>();
    web::FakeWebState* inserted_web_state = fake_web_state.get();
    SnapshotTabHelper::CreateForWebState(inserted_web_state);
    browser->GetWebStateList()->InsertWebState(
        std::move(fake_web_state),
        WebStateList::InsertionParams::Automatic().Activate());
    return inserted_web_state;
  }

  // Returns the tab ID for the web state at `index` in `browser`.
  web::WebStateID GetTabIDForWebStateAt(int index, Browser* browser) {
    web::WebState* web_state = browser->GetWebStateList()->GetWebStateAt(index);
    return web_state->GetUniqueIdentifier();
  }

  // Returns the cached snapshot for the given snapshot ID in the given snapshot
  // cache.
  UIImage* GetSnapshot(SnapshotStorageWrapper* snapshot_storage,
                       SnapshotID snapshot_id) {
    CHECK(snapshot_storage);
    base::RunLoop run_loop;
    base::RunLoop* run_loop_ptr = &run_loop;

    __block UIImage* snapshot = nil;
    [snapshot_storage retrieveImageForSnapshotID:snapshot_id
                                        callback:^(UIImage* cached_snapshot) {
                                          snapshot = cached_snapshot;
                                          run_loop_ptr->Quit();
                                        }];
    run_loop.Run();
    return snapshot;
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<Browser> browser_;
  std::unique_ptr<Browser> other_browser_;
  std::unique_ptr<Browser> incognito_browser_;
  std::unique_ptr<Browser> other_incognito_browser_;
  raw_ptr<BrowserList> browser_list_;
  raw_ptr<sessions::TabRestoreService> tab_restore_service_;
};

// Tests that an incognito tab is moved from one incognito browser to another.
TEST_F(BrowserUtilTest, TestMoveTabAcrossIncognitoBrowsers) {
  ASSERT_EQ(1, incognito_browser_->GetWebStateList()->count());
  ASSERT_TRUE(other_incognito_browser_->GetWebStateList()->empty());
  ASSERT_TRUE(tab_restore_service_->entries().empty());
  web::WebStateID tab_id = GetTabIDForWebStateAt(0, incognito_browser_.get());

  BrowserAndIndex tab_info = FindBrowserAndIndex(
      tab_id,
      browser_list_->BrowsersOfType(BrowserList::BrowserType::kIncognito));
  ASSERT_EQ(tab_info.tab_index, 0);
  ASSERT_EQ(tab_info.browser, incognito_browser_.get());

  MoveTabToBrowser(tab_id, other_incognito_browser_.get(),
                   /*destination_index=*/0);
  ASSERT_TRUE(tab_restore_service_->entries().empty());
  EXPECT_TRUE(incognito_browser_->GetWebStateList()->empty());
  EXPECT_EQ(1, other_incognito_browser_->GetWebStateList()->count());
}

// Tests that a tab is moved from one regular browser (with several tabs) to
// another browser.
TEST_F(BrowserUtilTest, TestMoveTabAcrossRegularBrowsers) {
  ASSERT_EQ(3, browser_->GetWebStateList()->count());
  ASSERT_TRUE(other_browser_->GetWebStateList()->empty());
  ASSERT_TRUE(tab_restore_service_->entries().empty());
  web::WebStateID tab_id = GetTabIDForWebStateAt(1, browser_.get());

  BrowserAndIndex tab_info = FindBrowserAndIndex(
      tab_id, browser_list_->BrowsersOfType(
                  BrowserList::BrowserType::kRegularAndInactive));
  ASSERT_EQ(tab_info.tab_index, 1);
  ASSERT_EQ(tab_info.browser, browser_.get());

  MoveTabToBrowser(tab_id, other_browser_.get(), /*destination_index=*/0);
  ASSERT_TRUE(tab_restore_service_->entries().empty());
  EXPECT_EQ(2, browser_->GetWebStateList()->count());
  EXPECT_EQ(1, other_browser_->GetWebStateList()->count());
  EXPECT_NE(tab_id, GetTabIDForWebStateAt(1, browser_.get()));
  EXPECT_EQ(tab_id, GetTabIDForWebStateAt(0, other_browser_.get()));
}

// Tests `FindBrowserAndIndex:` with an unknown tab_id.
TEST_F(BrowserUtilTest, TestFindBrowserAndIndexWithUnknownId) {
  web::WebStateID tab_id = web::WebStateID::NewUnique();

  BrowserAndIndex tab_info = FindBrowserAndIndex(
      tab_id, browser_list_->BrowsersOfType(
                  BrowserList::BrowserType::kRegularAndInactive));
  ASSERT_EQ(tab_info.tab_index, WebStateList::kInvalidIndex);
  EXPECT_NE(tab_info.browser, browser_.get());

  tab_info = FindBrowserAndIndex(
      tab_id,
      browser_list_->BrowsersOfType(BrowserList::BrowserType::kIncognito));
  ASSERT_EQ(tab_info.tab_index, WebStateList::kInvalidIndex);
  EXPECT_NE(tab_info.browser, incognito_browser_.get());
}

// Tests that a tab is reordered within the same browser.
TEST_F(BrowserUtilTest, TestReorderTabWithinSameBrowser) {
  ASSERT_EQ(3, browser_->GetWebStateList()->count());
  ASSERT_TRUE(tab_restore_service_->entries().empty());
  web::WebStateID tab_id = GetTabIDForWebStateAt(0, browser_.get());

  BrowserAndIndex tab_info = FindBrowserAndIndex(
      tab_id, browser_list_->BrowsersOfType(
                  BrowserList::BrowserType::kRegularAndInactive));
  ASSERT_EQ(tab_info.tab_index, 0);
  ASSERT_EQ(tab_info.browser, browser_.get());

  MoveTabToBrowser(tab_id, browser_.get(), /*destination_index=*/2);
  ASSERT_TRUE(tab_restore_service_->entries().empty());
  EXPECT_EQ(3, browser_->GetWebStateList()->count());
  EXPECT_NE(tab_id, GetTabIDForWebStateAt(0, browser_.get()));
  EXPECT_EQ(tab_id, GetTabIDForWebStateAt(2, browser_.get()));
}

// Tests that snapshots are correctly moved when moving a web state from active
// to inactive browser.
TEST_F(BrowserUtilTest, TestMovedSnapshot) {
  // Set a snapshot to the first web state of `browser_`.
  web::WebState* web_state = browser_->GetWebStateList()->GetWebStateAt(0);
  SnapshotBrowserAgent::CreateForBrowser(browser_.get());
  SnapshotBrowserAgent* agent =
      SnapshotBrowserAgent::FromBrowser(browser_.get());
  agent->SetSessionID(kIdentifier0);
  SnapshotStorageWrapper* snapshot_storage = agent->snapshot_storage();
  ASSERT_NE(nil, snapshot_storage);
  UIImage* snapshot = UIImageWithSizeAndSolidColor({10, 20}, UIColor.redColor);
  ASSERT_NE(nil, snapshot);
  SnapshotTabHelper* snapshot_tab_helper =
      SnapshotTabHelper::FromWebState(web_state);
  SnapshotID snapshot_id = snapshot_tab_helper->GetSnapshotID();
  [snapshot_storage setImage:snapshot withSnapshotID:snapshot_id];
  ASSERT_TRUE(
      UIImagesAreEqual(snapshot, GetSnapshot(snapshot_storage, snapshot_id)));
  // Check that the other browser doesn’t have a snapshot for that identifier.
  SnapshotBrowserAgent::CreateForBrowser(other_browser_.get());
  SnapshotBrowserAgent* other_agent =
      SnapshotBrowserAgent::FromBrowser(other_browser_.get());
  other_agent->SetSessionID(kIdentifier1);
  SnapshotStorageWrapper* other_snapshot_storage =
      other_agent->snapshot_storage();
  ASSERT_NE(nil, other_snapshot_storage);
  ASSERT_EQ(nil, GetSnapshot(other_snapshot_storage, snapshot_id));

  // Migrate the tab between browsers.
  MoveTabFromBrowserToBrowser(browser_.get(), 0, other_browser_.get(), 0);

  EXPECT_EQ(nil, GetSnapshot(snapshot_storage, snapshot_id));
  EXPECT_TRUE(UIImagesAreEqual(
      snapshot, GetSnapshot(other_snapshot_storage, snapshot_id)));
}
