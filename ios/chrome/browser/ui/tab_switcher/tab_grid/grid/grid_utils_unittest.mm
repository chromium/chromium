// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_utils.h"

#import "base/memory/raw_ptr.h"
#import "base/numerics/safe_conversions.h"
#import "components/tab_groups/tab_group_color.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_item_identifier.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_item.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "ui/base/device_form_factor.h"

using tab_groups::TabGroupVisualData;

class GridUtilsTest : public PlatformTest {
 public:
  GridUtilsTest() {
    TestChromeBrowserState::Builder browser_state_builder;
    browser_state_ = browser_state_builder.Build();
    browser_ = std::make_unique<TestBrowser>(
        browser_state_.get(), std::make_unique<FakeWebStateListDelegate>());
    web_state_list_ = browser_->GetWebStateList();
  }

  void AddWebState() {
    auto web_state = std::make_unique<web::FakeWebState>();
    web_state_list_->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::Automatic().Activate());
  }

  void AddPinnedWebState() {
    auto web_state = std::make_unique<web::FakeWebState>();
    web_state_list_->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::Automatic().Pinned());
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<TestBrowser> browser_;
  raw_ptr<WebStateList> web_state_list_;
};

TEST_F(GridUtilsTest, CreateValidItemsList) {
  AddWebState();
  AddWebState();
  AddWebState();
  AddWebState();
  AddWebState();

  NSArray<GridItemIdentifier*>* itemsList = CreateItems(web_state_list_);
  ASSERT_EQ(base::checked_cast<NSUInteger>(web_state_list_->count()),
            [itemsList count]);
  for (NSUInteger i = 0; i < [itemsList count]; i++) {
    EXPECT_EQ(GridItemType::Tab, itemsList[i].type);
    EXPECT_EQ(web_state_list_->GetWebStateAt(i)->GetUniqueIdentifier(),
              itemsList[i].tabSwitcherItem.identifier);
  }
}

TEST_F(GridUtilsTest, CreateValidItemsListWithoutPinnedTabs) {
  // The Pinned Tabs feature is not available on iPad.
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    return;
  }
  AddPinnedWebState();
  AddWebState();
  AddPinnedWebState();
  AddWebState();
  AddPinnedWebState();

  NSArray<GridItemIdentifier*>* itemsList = CreateItems(web_state_list_);
  ASSERT_EQ(base::checked_cast<NSUInteger>(web_state_list_->count()) -
                web_state_list_->pinned_tabs_count(),
            [itemsList count]);
  NSInteger number_of_pinned_tabs = web_state_list_->pinned_tabs_count();
  for (NSUInteger i = 0; i < [itemsList count]; i++) {
    web::WebState* web_state =
        web_state_list_->GetWebStateAt(i + number_of_pinned_tabs);
    GridItemIdentifier* item = itemsList[i];
    EXPECT_EQ(GridItemType::Tab, item.type);
    EXPECT_EQ(web_state->GetUniqueIdentifier(),
              itemsList[i].tabSwitcherItem.identifier);
  }
}

// Test that `-CreateItems` handles the creation of different item types (groups
// and tabs) when the `web_state_list_` contains groups.
TEST_F(GridUtilsTest, CreateItemsListWithGroup) {
  for (int i = 0; i < 10; i++) {
    AddWebState();
  }
  TabGroupVisualData visual_data_a =
      TabGroupVisualData(u"Group A", tab_groups::TabGroupColorId::kGrey);
  TabGroupVisualData visual_data_b =
      TabGroupVisualData(u"Group B", tab_groups::TabGroupColorId::kRed);

  web_state_list_->CreateGroup({0, 1, 2}, visual_data_a);
  web_state_list_->CreateGroup({5, 6}, visual_data_b);

  NSArray<GridItemIdentifier*>* itemsList = CreateItems(web_state_list_);

  // The number of items should be equal to 7, 2 groups ({0, 1, 2} and {5, 6})
  // and 5 web states({3,4,7,8,9}).
  ASSERT_EQ(7, (int)[itemsList count]);
  EXPECT_EQ(GridItemType::Group, itemsList[0].type);
  EXPECT_EQ(GridItemType::Tab, itemsList[1].type);
  EXPECT_EQ(GridItemType::Tab, itemsList[2].type);
  EXPECT_EQ(GridItemType::Group, itemsList[3].type);
  EXPECT_EQ(GridItemType::Tab, itemsList[4].type);
  EXPECT_EQ(GridItemType::Tab, itemsList[5].type);
  EXPECT_EQ(GridItemType::Tab, itemsList[6].type);
}
