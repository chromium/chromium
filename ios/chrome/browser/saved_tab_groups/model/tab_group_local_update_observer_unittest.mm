// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/saved_tab_groups/model/tab_group_local_update_observer.h"

#import <memory>
#import <optional>
#import <string>

#import "base/memory/raw_ptr.h"
#import "base/uuid.h"
#import "components/saved_tab_groups/mock_tab_group_sync_service.h"
#import "components/saved_tab_groups/saved_tab_group.h"
#import "components/saved_tab_groups/saved_tab_group_tab.h"
#import "components/saved_tab_groups/types.h"
#import "components/tab_groups/tab_group_color.h"
#import "components/tab_groups/tab_group_id.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/web_state_list_builder_from_description.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_id.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

using testing::_;

namespace tab_groups {

namespace {

std::unique_ptr<KeyedService> CreateMockSyncService(
    web::BrowserState* context) {
  return std::make_unique<MockTabGroupSyncService>();
}

}  // namespace

class TabGroupLocalUpdateObserverTest : public PlatformTest {
 public:
  TabGroupLocalUpdateObserverTest() {
    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(TabGroupSyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateMockSyncService));
    browser_state_ = builder.Build();

    mock_service_ = static_cast<MockTabGroupSyncService*>(
        TabGroupSyncServiceFactory::GetForBrowserState(browser_state_.get()));

    browser_ = std::make_unique<TestBrowser>(
        browser_state_.get(), nil,
        std::make_unique<FakeWebStateListDelegate>());
    browser_same_browser_state_ = std::make_unique<TestBrowser>(
        browser_state_.get(), nil,
        std::make_unique<FakeWebStateListDelegate>());

    other_browser_state_ = TestChromeBrowserState::Builder().Build();
    other_browser_ = std::make_unique<TestBrowser>(
        other_browser_state_.get(), nil,
        std::make_unique<FakeWebStateListDelegate>());

    browser_list_ =
        BrowserListFactory::GetForBrowserState(browser_state_.get());
    browser_list_->AddBrowser(browser_.get());
    local_observer_ = std::make_unique<TabGroupLocalUpdateObserver>(
        browser_list_.get(), mock_service_);

    BrowserList* other_browser_list =
        BrowserListFactory::GetForBrowserState(other_browser_state_.get());
    other_browser_list->AddBrowser(other_browser_.get());
  }

  // Returns a new fake web state.
  std::unique_ptr<web::FakeWebState> CreateWebState() {
    std::unique_ptr<web::FakeWebState> web_state =
        std::make_unique<web::FakeWebState>(web::WebStateID::NewUnique());
    web_state->SetCurrentURL(GURL("http://first-url.com"));
    web_state->SetTitle(u"original title");
    return web_state;
  }

  // Inserts a new FakeWebState in `web_state_list`.
  web::FakeWebState* InsertWebState(WebStateList* web_state_list) {
    std::unique_ptr<web::FakeWebState> unique_web_state = CreateWebState();
    web::FakeWebState* web_state = unique_web_state.get();
    web_state_list->InsertWebState(std::move(unique_web_state));
    return web_state;
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<TestBrowser> browser_;
  std::unique_ptr<TestBrowser> browser_same_browser_state_;
  std::unique_ptr<TestChromeBrowserState> other_browser_state_;
  std::unique_ptr<TestBrowser> other_browser_;
  raw_ptr<BrowserList> browser_list_;
  std::unique_ptr<TabGroupLocalUpdateObserver> local_observer_;
  raw_ptr<MockTabGroupSyncService> mock_service_;
  const std::u16string kNewTitle = u"title to update";
};

// Tests that the service is correctly updated when the title of a tab that was
// added after creating the service is updated.
TEST_F(TabGroupLocalUpdateObserverTest, TitleUpdateExistingTab) {
  WebStateList* web_state_list = browser_->GetWebStateList();

  web::FakeWebState* web_state = InsertWebState(web_state_list);
  web::WebStateID web_state_id = web_state->GetUniqueIdentifier();

  TabGroupId tab_group_id = TabGroupId::GenerateNew();
  web_state_list->CreateGroup({0}, {}, tab_group_id);

  EXPECT_CALL(*mock_service_, UpdateTab(tab_group_id, web_state_id.identifier(),
                                        kNewTitle, _, std::make_optional(0ul)))
      .Times(1);

  web_state->SetTitle(kNewTitle);
}

// Tests that the service is correctly updated when the title of a tab that was
// existing when creating the service is updated.
TEST_F(TabGroupLocalUpdateObserverTest, TitleUpdateNewTab) {
  WebStateList* web_state_list = browser_->GetWebStateList();

  web::FakeWebState* web_state = InsertWebState(web_state_list);
  web::WebStateID web_state_id = web_state->GetUniqueIdentifier();

  TabGroupId tab_group_id = TabGroupId::GenerateNew();
  web_state_list->CreateGroup({0}, {}, tab_group_id);

  EXPECT_CALL(*mock_service_, UpdateTab(tab_group_id, web_state_id.identifier(),
                                        kNewTitle, _, std::make_optional(0ul)));

  web_state->SetTitle(kNewTitle);
}

// Tests that the service is correctly updated when the title of a tab that is
// in a WebStateList that was added after the service creation is updated.
TEST_F(TabGroupLocalUpdateObserverTest, TitleUpdateNewWebStateList) {
  WebStateList* web_state_list = browser_same_browser_state_->GetWebStateList();

  web::FakeWebState* web_state = InsertWebState(web_state_list);
  web::WebStateID web_state_id = web_state->GetUniqueIdentifier();

  TabGroupId tab_group_id = TabGroupId::GenerateNew();
  web_state_list->CreateGroup({0}, {}, tab_group_id);

  // Add the Browser after the tab is inserted.
  BrowserListFactory::GetForBrowserState(browser_state_.get())
      ->AddBrowser(browser_same_browser_state_.get());

  EXPECT_CALL(*mock_service_, UpdateTab(tab_group_id, web_state_id.identifier(),
                                        kNewTitle, _, std::make_optional(0ul)));

  web_state->SetTitle(kNewTitle);
}

// Tests that the service is correctly updated when the title of a tab that
// inserted in a WebStateList that was added after the service creation is
// updated.
TEST_F(TabGroupLocalUpdateObserverTest, TitleUpdateNewWebStateListInsert) {
  // Add the browser before inserting the tab.
  BrowserListFactory::GetForBrowserState(browser_state_.get())
      ->AddBrowser(browser_same_browser_state_.get());

  WebStateList* web_state_list = browser_same_browser_state_->GetWebStateList();

  std::unique_ptr<web::FakeWebState> unique_web_state = CreateWebState();
  web::WebStateID web_state_id = unique_web_state->GetUniqueIdentifier();
  web::FakeWebState* web_state = unique_web_state.get();
  web_state_list->InsertWebState(std::move(unique_web_state));

  TabGroupId tab_group_id = TabGroupId::GenerateNew();
  web_state_list->CreateGroup({0}, {}, tab_group_id);

  EXPECT_CALL(*mock_service_, UpdateTab(tab_group_id, web_state_id.identifier(),
                                        kNewTitle, _, std::make_optional(0ul)));

  web_state->SetTitle(kNewTitle);
}

}  // namespace tab_groups
