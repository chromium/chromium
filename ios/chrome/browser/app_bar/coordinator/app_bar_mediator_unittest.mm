// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_bar/coordinator/app_bar_mediator.h"

#import <memory>

#import "ios/chrome/browser/app_bar/ui/app_bar_consumer.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_grid_paging.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class AppBarMediatorTest : public PlatformTest {
 protected:
  AppBarMediatorTest() {
    regular_web_state_list_ =
        std::make_unique<WebStateList>(&regular_web_state_list_delegate_);
    incognito_web_state_list_ =
        std::make_unique<WebStateList>(&incognito_web_state_list_delegate_);

    mediator_ = [[AppBarMediator alloc]
        initWithRegularWebStateList:regular_web_state_list_.get()
              incognitoWebStateList:incognito_web_state_list_.get()];

    consumer_ = OCMProtocolMock(@protocol(AppBarConsumer));
    mediator_.consumer = consumer_;
  }

  ~AppBarMediatorTest() override { [mediator_ disconnect]; }

  AppBarMediator* mediator_;
  std::unique_ptr<WebStateList> regular_web_state_list_;
  std::unique_ptr<WebStateList> incognito_web_state_list_;
  FakeWebStateListDelegate regular_web_state_list_delegate_;
  FakeWebStateListDelegate incognito_web_state_list_delegate_;
  id consumer_;
};

// Tests that the consumer is updated when a web state is added.
TEST_F(AppBarMediatorTest, TestDidAddWebState) {
  OCMExpect([consumer_ updateTabCount:1]);
  auto web_state = std::make_unique<web::FakeWebState>();
  regular_web_state_list_->InsertWebState(std::move(web_state));
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the consumer is updated when a web state is detached.
TEST_F(AppBarMediatorTest, TestDidDetachWebState) {
  auto web_state = std::make_unique<web::FakeWebState>();
  regular_web_state_list_->InsertWebState(std::move(web_state));

  OCMExpect([consumer_ updateTabCount:0]);
  regular_web_state_list_->DetachWebStateAt(0);
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the consumer is updated when switching to incognito.
TEST_F(AppBarMediatorTest, TestSwitchToIncognito) {
  // Add a web state to incognito.
  auto web_state = std::make_unique<web::FakeWebState>();
  incognito_web_state_list_->InsertWebState(std::move(web_state));

  // Switch to incognito.
  OCMExpect([consumer_ updateTabCount:1]);
  [mediator_ willChangePageTo:TabGridPageIncognitoTabs];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the consumer is updated when switching back to regular.
TEST_F(AppBarMediatorTest, TestSwitchToRegular) {
  // Add a web state to regular.
  auto web_state = std::make_unique<web::FakeWebState>();
  regular_web_state_list_->InsertWebState(std::move(web_state));

  // Switch to incognito (empty).
  OCMExpect([consumer_ updateTabCount:0]);
  [mediator_ willChangePageTo:TabGridPageIncognitoTabs];
  EXPECT_OCMOCK_VERIFY(consumer_);

  // Switch back to regular.
  OCMExpect([consumer_ updateTabCount:1]);
  [mediator_ willChangePageTo:TabGridPageRegularTabs];
  EXPECT_OCMOCK_VERIFY(consumer_);
}
