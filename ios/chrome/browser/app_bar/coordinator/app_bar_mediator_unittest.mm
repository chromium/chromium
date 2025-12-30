// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_bar/coordinator/app_bar_mediator.h"

#import <memory>

#import "ios/chrome/browser/app_bar/ui/app_bar_consumer.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class AppBarMediatorTest : public PlatformTest {
 protected:
  AppBarMediatorTest() {
    mediator_ = [[AppBarMediator alloc] init];
    web_state_list_ = std::make_unique<WebStateList>(&web_state_list_delegate_);
    mediator_.webStateList = web_state_list_.get();
    consumer_ = OCMProtocolMock(@protocol(AppBarConsumer));
    mediator_.consumer = consumer_;
  }

  ~AppBarMediatorTest() override { [mediator_ disconnect]; }

  AppBarMediator* mediator_;
  std::unique_ptr<WebStateList> web_state_list_;
  FakeWebStateListDelegate web_state_list_delegate_;
  id consumer_;
};

// Tests that the consumer is updated when a web state is added.
TEST_F(AppBarMediatorTest, TestDidAddWebState) {
  OCMExpect([consumer_ updateTabCount:1]);
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state_list_->InsertWebState(std::move(web_state));
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the consumer is updated when a web state is detached.
TEST_F(AppBarMediatorTest, TestDidDetachWebState) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state_list_->InsertWebState(std::move(web_state));

  OCMExpect([consumer_ updateTabCount:0]);
  web_state_list_->DetachWebStateAt(0);
  EXPECT_OCMOCK_VERIFY(consumer_);
}
