// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/group_utils.h"

#import "components/favicon/core/favicon_service.h"
#import "components/favicon/core/favicon_url.h"
#import "components/favicon/ios/web_favicon_driver.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/tab_groups/tab_group_visual_data.h"
#import "ios/chrome/browser/favicon/model/favicon_service_factory.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

class GroupUtilsTest : public PlatformTest {
 public:
  GroupUtilsTest() {
    TestChromeBrowserState::Builder browser_state_builder;
    browser_state_builder.AddTestingFactory(
        ios::FaviconServiceFactory::GetInstance(),
        ios::FaviconServiceFactory::GetDefaultFactory());
    browser_state_builder.AddTestingFactory(
        ios::HistoryServiceFactory::GetInstance(),
        ios::HistoryServiceFactory::GetDefaultFactory());

    browser_state_ = browser_state_builder.Build();
    browser_ = std::make_unique<TestBrowser>(
        browser_state_.get(), std::make_unique<FakeWebStateListDelegate>());
    web_state_list_ = browser_->GetWebStateList();
  }

  void AddWebState() {
    auto web_state = std::make_unique<web::FakeWebState>();
    web_state->SetBrowserState(browser_state_.get());
    favicon::WebFaviconDriver::CreateForWebState(
        web_state.get(),
        ios::FaviconServiceFactory::GetForBrowserState(
            browser_state_.get(), ServiceAccessType::IMPLICIT_ACCESS));

    web_state_list_->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::Automatic().Activate());
  }

  void CreateGroup(int web_state_index, tab_groups::TabGroupColorId color) {
    tab_groups::TabGroupVisualData visual_data(u"Test title", color);
    web_state_list_->CreateGroup({web_state_index}, visual_data);
  }

  tab_groups::TabGroupColorId DefaultColor() {
    return DefaultColorForNewTabGroup(web_state_list_.get());
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<TestBrowser> browser_;
  raw_ptr<WebStateList> web_state_list_;
};

TEST_F(GroupUtilsTest, TestDefaultColor) {
  std::vector<tab_groups::TabGroupColorId> colors = AllPossibleTabGroupColors();

  for (unsigned int i = 0; i < colors.size() * 3 + 1; i++) {
    AddWebState();
  }

  EXPECT_EQ(colors[0], DefaultColor());

  // Check the first two independently to make sure that even if the first web
  // state is not in a group it is still working.
  CreateGroup(1, colors[0]);
  EXPECT_EQ(colors[1], DefaultColor());

  CreateGroup(0, colors[1]);
  EXPECT_EQ(colors[2], DefaultColor());

  // Check the following colors.
  for (unsigned int i = 2; i < colors.size() - 1; i++) {
    CreateGroup(i, colors[i]);
    EXPECT_EQ(colors[i + 1], DefaultColor());
  }

  // Check the last one indenpendently. It should cycle.
  CreateGroup(colors.size() - 1, colors[colors.size() - 1]);
  EXPECT_EQ(colors[0], DefaultColor());

  // Cycle again.
  CreateGroup(colors.size(), colors[0]);
  EXPECT_EQ(colors[1], DefaultColor());
}
