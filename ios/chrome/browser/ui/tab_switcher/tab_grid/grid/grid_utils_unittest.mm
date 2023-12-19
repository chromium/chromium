// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_utils.h"

#import "base/numerics/safe_conversions.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_item.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "ui/base/device_form_factor.h"

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
    web_state_list_->InsertWebState(0, std::move(web_state),
                                    WebStateList::INSERT_ACTIVATE,
                                    WebStateOpener());
  }

  void AddPinnedWebState() {
    auto web_state = std::make_unique<web::FakeWebState>();
    web_state_list_->InsertWebState(
        0, std::move(web_state), WebStateList::INSERT_PINNED, WebStateOpener());
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<TestBrowser> browser_;
  WebStateList* web_state_list_;
};

TEST_F(GridUtilsTest, CreateValidItemsList) {
  AddWebState();
  AddWebState();
  AddWebState();
  AddWebState();
  AddWebState();

  NSArray<TabSwitcherItem*>* itemsList = CreateItems(web_state_list_);
  ASSERT_EQ(base::checked_cast<NSUInteger>(web_state_list_->count()),
            [itemsList count]);
  for (NSUInteger i = 0; i < [itemsList count]; i++) {
    EXPECT_EQ(web_state_list_->GetWebStateAt(i)->GetUniqueIdentifier(),
              itemsList[i].identifier);
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

  NSArray<TabSwitcherItem*>* itemsList = CreateItems(web_state_list_);
  ASSERT_EQ(base::checked_cast<NSUInteger>(web_state_list_->count()) -
                web_state_list_->pinned_tabs_count(),
            [itemsList count]);
  for (NSUInteger i = web_state_list_->pinned_tabs_count();
       i < [itemsList count]; i++) {
    EXPECT_EQ(web_state_list_->GetWebStateAt(i)->GetUniqueIdentifier(),
              itemsList[i].identifier);
  }
}
