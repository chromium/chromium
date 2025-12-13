// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_snapshot_and_favicon_configurator.h"

#import "base/barrier_closure.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/run_loop.h"
#import "base/test/ios/wait_util.h"
#import "base/test/test_timeouts.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/favicon/model/test_favicon_loader.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/snapshots/model/snapshot_browser_agent.h"
#import "ios/chrome/browser/snapshots/model/snapshot_source_tab_helper.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_group_item.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_snapshot_and_favicon.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/web_state_tab_switcher_item.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

namespace {

// Builds a `TestFaviconLoader`.
std::unique_ptr<KeyedService> BuildTestFaviconLoader(ProfileIOS* profile) {
  return std::make_unique<TestFaviconLoader>();
}

// The identifier where the snapshots are saved.
const char kIdentifier[] = "Identifier";

}  // namespace

// Test fixture for TabSnapshotAndFaviconConfigurator.
class TabSnapshotAndFaviconConfiguratorTest : public PlatformTest {
 public:
  TabSnapshotAndFaviconConfiguratorTest() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(IOSChromeFaviconLoaderFactory::GetInstance(),
                              base::BindOnce(&BuildTestFaviconLoader));
    profile_ = std::move(builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    SnapshotBrowserAgent::CreateForBrowser(browser_.get());
    SnapshotBrowserAgent::FromBrowser(browser_.get())
        ->SetSessionID(kIdentifier);

    FaviconLoader* favicon_loader =
        IOSChromeFaviconLoaderFactory::GetForProfile(profile_.get());
    _configurator = std::make_unique<TabSnapshotAndFaviconConfigurator>(
        favicon_loader, SnapshotBrowserAgent::FromBrowser(browser_.get()));

    ConfigureWebStateList();
  }

 protected:
  // Configures the `web_state_list_` and `tab_group_item_`.
  void ConfigureWebStateList() {
    web_state_list_ = browser_->GetWebStateList();
    AppendNewWebState();
    AppendNewWebState();

    // Create a group of two tabs.
    tab_groups::TabGroupId tab_group_id = tab_groups::TabGroupId::GenerateNew();
    tab_groups::TabGroupVisualData visual_data = tab_groups::TabGroupVisualData(
        u"Group", tab_groups::TabGroupColorId::kGrey);
    const TabGroup* tab_group = web_state_list_->CreateGroup(
        {0, 1}, tab_groups::TabGroupVisualData(visual_data), tab_group_id);
    tab_group_item_ = [[TabGroupItem alloc] initWithTabGroup:tab_group];

    ASSERT_EQ(2, web_state_list_->count());
    ASSERT_EQ(tab_group, web_state_list_->GetGroupOfWebStateAt(0));
  }

  // Appends a new web state in `web_state_list_`.
  web::FakeWebState* AppendNewWebState() {
    auto fake_web_state = std::make_unique<web::FakeWebState>();
    web::FakeWebState* inserted_web_state = fake_web_state.get();
    SnapshotTabHelper::CreateForWebState(inserted_web_state);
    SnapshotSourceTabHelper::CreateForWebState(inserted_web_state);
    web_state_list_->InsertWebState(
        std::move(fake_web_state),
        WebStateList::InsertionParams::Automatic().Activate());
    return inserted_web_state;
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  TestFaviconLoader favicon_loader_;
  raw_ptr<WebStateList> web_state_list_;
  TabGroupItem* tab_group_item_;
  std::unique_ptr<TabSnapshotAndFaviconConfigurator> _configurator;
};

// Tests the default use case of `FetchSnapshotAndFaviconForTabGroupItem:`.
TEST_F(TabSnapshotAndFaviconConfiguratorTest,
       FetchSnapshotAndFaviconForTabGroupItem) {
  // Expect the completion to be called four times.
  base::RunLoop run_loop;
  auto barrier = base::CallbackToBlock(
      base::IgnoreArgs<TabGroupItem*, NSInteger, TabSnapshotAndFavicon*>(
          base::BarrierClosure(4, run_loop.QuitClosure())));

  _configurator->FetchSnapshotAndFaviconForTabGroupItem(
      tab_group_item_, web_state_list_, barrier);
  run_loop.Run();
}

// Tests the use case of `FetchSnapshotAndFaviconForTabGroupItem:` for a large
// group.
TEST_F(TabSnapshotAndFaviconConfiguratorTest,
       FetchSnapshotAndFaviconForTabGroupItemlargeGroup) {
  for (int index = 0; index < 8; index++) {
    AppendNewWebState();
  }
  web_state_list_->MoveToGroup({2, 3, 4, 5, 6, 7, 8, 9},
                               tab_group_item_.tabGroup);
  ASSERT_EQ(tab_group_item_.tabGroup->range().count(), 10);

  // Expect the completion to be called nine times.
  base::RunLoop run_loop;
  auto barrier = base::CallbackToBlock(
      base::IgnoreArgs<TabGroupItem*, NSInteger, TabSnapshotAndFavicon*>(
          base::BarrierClosure(9, run_loop.QuitClosure())));

  _configurator->FetchSnapshotAndFaviconForTabGroupItem(
      tab_group_item_, web_state_list_, barrier);
  run_loop.Run();
}

// Tests the default use case of `FetchSingleSnapshotAndFaviconFromWebState:`.
TEST_F(TabSnapshotAndFaviconConfiguratorTest,
       FetchSingleSnapshotAndFaviconFromWebState) {
  // Expect the completion to be called once.
  base::RunLoop run_loop;
  auto barrier = base::CallbackToBlock(base::IgnoreArgs<TabSnapshotAndFavicon*>(
      base::BarrierClosure(1, run_loop.QuitClosure())));

  _configurator->FetchSingleSnapshotAndFaviconFromWebState(
      web_state_list_->GetWebStateAt(0), barrier);
  run_loop.Run();
}

// Tests the default use case of `FetchSnapshotAndFaviconForTabSwitcherItem:`.
TEST_F(TabSnapshotAndFaviconConfiguratorTest,
       FetchSnapshotAndFaviconForTabSwitcherItem) {
  WebStateTabSwitcherItem* item = [[WebStateTabSwitcherItem alloc]
      initWithWebState:web_state_list_->GetWebStateAt(0)];

  // Expect the completion to be called twice.
  base::RunLoop run_loop;
  auto barrier = base::CallbackToBlock(
      base::IgnoreArgs<WebStateTabSwitcherItem*, TabSnapshotAndFavicon*>(
          base::BarrierClosure(2, run_loop.QuitClosure())));

  _configurator->FetchSnapshotAndFaviconForTabSwitcherItem(item, barrier);
  run_loop.Run();
}

// Tests the default use case of `FetchFaviconForTabSwitcherItem:`.
TEST_F(TabSnapshotAndFaviconConfiguratorTest, FetchFaviconForTabSwitcherItem) {
  WebStateTabSwitcherItem* item = [[WebStateTabSwitcherItem alloc]
      initWithWebState:web_state_list_->GetWebStateAt(0)];

  // Expect the completion to be called once.
  base::RunLoop run_loop;
  auto barrier = base::CallbackToBlock(
      base::IgnoreArgs<WebStateTabSwitcherItem*, TabSnapshotAndFavicon*>(
          base::BarrierClosure(1, run_loop.QuitClosure())));

  _configurator->FetchFaviconForTabSwitcherItem(item, barrier);
  run_loop.Run();
}
