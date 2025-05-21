// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_snapshot_and_favicon_configurator.h"

#import "base/test/ios/wait_util.h"
#import "base/test/test_timeouts.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/favicon/model/test_favicon_loader.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/group_tab_info.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_group_item.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

namespace {

// Builds a `TestFaviconLoader`.
std::unique_ptr<KeyedService> BuildTestFaviconLoader(
    web::BrowserState* context) {
  return std::make_unique<TestFaviconLoader>();
}

// Checks that the `GroupTabInfo` in `group_infos` are correctly populated.
void CheckGroupTabInfos(NSArray<GroupTabInfo*>* group_infos,
                        int expected_count) {
  ASSERT_EQ((int)group_infos.count, expected_count);
  for (GroupTabInfo* info in group_infos) {
    ASSERT_TRUE(info.favicon);
    ASSERT_TRUE(info.snapshot);
  }
}

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

    FaviconLoader* favicon_loader =
        IOSChromeFaviconLoaderFactory::GetForProfile(profile_.get());
    _configurator =
        std::make_unique<TabSnapshotAndFaviconConfigurator>(favicon_loader);

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

// Tests the default use case of `FetchGroupTabInfoForTabGroupItem:`.
TEST_F(TabSnapshotAndFaviconConfiguratorTest,
       FetchGroupTabInfoForTabGroupItem) {
  __block BOOL completion_block_called = NO;
  auto completion_block =
      ^(TabGroupItem* inner_item, NSArray<GroupTabInfo*>* group_tab_infos) {
        ASSERT_EQ(inner_item, tab_group_item_);
        CheckGroupTabInfos(group_tab_infos, 2);
        completion_block_called = YES;
      };
  _configurator->FetchGroupTabInfoForTabGroupItem(
      tab_group_item_, web_state_list_, completion_block);
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      TestTimeouts::action_timeout(), ^bool() {
        return completion_block_called;
      }));
}

// Tests the default use case of `FetchSingleGroupTabInfoFromWebState:`.
TEST_F(TabSnapshotAndFaviconConfiguratorTest,
       FetchSingleGroupTabInfoFromWebState) {
  __block BOOL completion_block_called = NO;
  auto completion_block = ^(GroupTabInfo* tab_info) {
    CheckGroupTabInfos(@[ tab_info ], 1);
    completion_block_called = YES;
  };
  _configurator->FetchSingleGroupTabInfoFromWebState(
      web_state_list_->GetWebStateAt(0), completion_block);
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      TestTimeouts::action_timeout(), ^bool() {
        return completion_block_called;
      }));
}
