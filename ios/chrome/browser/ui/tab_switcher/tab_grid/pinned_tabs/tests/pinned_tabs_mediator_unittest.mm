// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/pinned_tabs/pinned_tabs_mediator.h"

#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/drag_and_drop/drag_item_util.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/tabs/features.h"
#import "ios/chrome/browser/ui/tab_switcher/test/fake_drag_session.h"
#import "ios/chrome/browser/ui/tab_switcher/test/fake_drop_session.h"
#import "ios/chrome/browser/ui/tab_switcher/test/fake_tab_collection_consumer.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "ui/base/device_form_factor.h"

namespace {

// Returns a GURL for the given `index`.
GURL GURLWithIndex(int index) {
  return GURL("http://test/url" + base::NumberToString(index));
}

// Returns a FakeDropSession for the given `web_state`.
FakeDropSession* FakeDropSessionWithWebState(web::WebState* web_state) {
  UIDragItem* drag_item = CreateTabDragItem(web_state);
  FakeDropSession* drop_session =
      [[FakeDropSession alloc] initWithItems:@[ drag_item ]];
  return drop_session;
}

}  // namespace

class PinnedTabsMediatorTest : public PlatformTest {
 public:
  PinnedTabsMediatorTest() {
    TestChromeBrowserState::Builder builder;
    browser_state_ = builder.Build();

    regular_browser_ = std::make_unique<TestBrowser>(browser_state_.get());
    incognito_browser_ = std::make_unique<TestBrowser>(
        browser_state_->GetOffTheRecordChromeBrowserState());

    browser_list_ =
        BrowserListFactory::GetForBrowserState(browser_state_.get());
    browser_list_->AddBrowser(regular_browser_.get());
    browser_list_->AddIncognitoBrowser(incognito_browser_.get());

    feature_list_.InitAndEnableFeature(kEnablePinnedTabs);

    // The Pinned Tabs feature is not available on iPad.
    if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_TABLET) {
      consumer_ = [[FakeTabCollectionConsumer alloc] init];
      mediator_ = [[PinnedTabsMediator alloc] initWithConsumer:consumer_];
      mediator_.browser = regular_browser_.get();
    }
  }

  // Creates a FakeWebState with a navigation history containing exactly only
  // the given `url`.
  std::unique_ptr<web::FakeWebState> CreateFakeWebStateWithURL(
      const GURL& url) {
    auto web_state = std::make_unique<web::FakeWebState>();
    auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
    navigation_manager->AddItem(url, ui::PAGE_TRANSITION_LINK);
    navigation_manager->SetLastCommittedItem(
        navigation_manager->GetItemAtIndex(0));
    web_state->SetNavigationManager(std::move(navigation_manager));
    web_state->SetBrowserState(browser_state_.get());
    web_state->SetNavigationItemCount(1);
    web_state->SetCurrentURL(url);
    return web_state;
  }

 protected:
  std::unique_ptr<TestBrowser> regular_browser_;
  std::unique_ptr<Browser> incognito_browser_;
  FakeTabCollectionConsumer* consumer_;
  PinnedTabsMediator* mediator_;

 private:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  BrowserList* browser_list_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
};

// Tests that the consumer is notified when a web state is pinned.
TEST_F(PinnedTabsMediatorTest, ConsumerInsertItem) {
  // The Pinned Tabs feature is not available on iPad.
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    return;
  }

  // Inserts two new pinned tabs.
  auto web_state1 = CreateFakeWebStateWithURL(GURLWithIndex(1));
  regular_browser_->GetWebStateList()->InsertWebState(
      0, std::move(web_state1), WebStateList::INSERT_PINNED, WebStateOpener());
  auto web_state2 = CreateFakeWebStateWithURL(GURLWithIndex(2));
  regular_browser_->GetWebStateList()->InsertWebState(
      0, std::move(web_state2), WebStateList::INSERT_PINNED, WebStateOpener());
  EXPECT_EQ(2UL, consumer_.items.count);

  // Inserts one regular and one incoginto tab.
  auto web_state3 = CreateFakeWebStateWithURL(GURLWithIndex(3));
  regular_browser_->GetWebStateList()->InsertWebState(
      0, std::move(web_state3), WebStateList::INSERT_FORCE_INDEX,
      WebStateOpener());
  auto web_state4 = CreateFakeWebStateWithURL(GURLWithIndex(4));
  incognito_browser_->GetWebStateList()->InsertWebState(
      0, std::move(web_state4), WebStateList::INSERT_FORCE_INDEX,
      WebStateOpener());
  EXPECT_EQ(2UL, consumer_.items.count);

  // Inserts a third pinned tab.
  auto web_state5 = CreateFakeWebStateWithURL(GURLWithIndex(5));
  regular_browser_->GetWebStateList()->InsertWebState(
      0, std::move(web_state5), WebStateList::INSERT_PINNED, WebStateOpener());
  EXPECT_EQ(3UL, consumer_.items.count);
}

// Tests that the correct UIDropOperation is returned when dropping tabs in the
// pinned view.
TEST_F(PinnedTabsMediatorTest, DropOperation) {
  // The Pinned Tabs feature is not available on iPad.
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    return;
  }

  // Tests a regular tab.
  auto regular_web_state = CreateFakeWebStateWithURL(GURLWithIndex(1));
  regular_browser_->GetWebStateList()->InsertWebState(
      0, std::move(regular_web_state), WebStateList::INSERT_FORCE_INDEX,
      WebStateOpener());

  FakeDropSession* regular_drop_session = FakeDropSessionWithWebState(
      regular_browser_->GetWebStateList()->GetWebStateAt(0));
  EXPECT_EQ([mediator_ dropOperationForDropSession:regular_drop_session],
            UIDropOperationMove);

  // Tests an incognito tab.
  auto incognito_web_state = CreateFakeWebStateWithURL(GURLWithIndex(2));
  incognito_browser_->GetWebStateList()->InsertWebState(
      0, std::move(incognito_web_state), WebStateList::INSERT_FORCE_INDEX,
      WebStateOpener());
  FakeDropSession* incognito_drop_session = FakeDropSessionWithWebState(
      incognito_browser_->GetWebStateList()->GetWebStateAt(0));
  EXPECT_EQ([mediator_ dropOperationForDropSession:incognito_drop_session],
            UIDropOperationMove);
}
