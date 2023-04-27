// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/commerce/session_proto_db_factory.h"

#import "components/commerce/core/proto/commerce_subscription_db_content.pb.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class SessionProtoDBFactoryTest : public PlatformTest {
 public:
  SessionProtoDBFactoryTest() {}

  void SetUp() override {
    TestChromeBrowserState::Builder builder_a;
    browser_state_a_ = builder_a.Build();
    TestChromeBrowserState::Builder builder_b;
    browser_state_b_ = builder_b.Build();
  }

 protected:
  std::unique_ptr<TestChromeBrowserState> browser_state_a_;
  std::unique_ptr<TestChromeBrowserState> browser_state_b_;
  web::WebTaskEnvironment task_environment_;
};

TEST_F(SessionProtoDBFactoryTest, TestIncognito) {
  EXPECT_EQ(
      nullptr,
      SessionProtoDBFactory<commerce_subscription_db::
                                CommerceSubscriptionContentProto>::GetInstance()
          ->GetForBrowserState(
              browser_state_a_->GetOffTheRecordChromeBrowserState()));
}

TEST_F(SessionProtoDBFactoryTest, TestNonIncognito) {
  EXPECT_NE(
      nullptr,
      SessionProtoDBFactory<commerce_subscription_db::
                                CommerceSubscriptionContentProto>::GetInstance()
          ->GetForBrowserState(browser_state_a_.get()));
}

TEST_F(SessionProtoDBFactoryTest, TestSameBrowserState) {
  EXPECT_EQ(
      SessionProtoDBFactory<commerce_subscription_db::
                                CommerceSubscriptionContentProto>::GetInstance()
          ->GetForBrowserState(browser_state_a_.get()),
      SessionProtoDBFactory<commerce_subscription_db::
                                CommerceSubscriptionContentProto>::GetInstance()
          ->GetForBrowserState(browser_state_a_.get()));
}

TEST_F(SessionProtoDBFactoryTest, TestDifferentBrowserState) {
  EXPECT_NE(
      SessionProtoDBFactory<commerce_subscription_db::
                                CommerceSubscriptionContentProto>::GetInstance()
          ->GetForBrowserState(browser_state_a_.get()),
      SessionProtoDBFactory<commerce_subscription_db::
                                CommerceSubscriptionContentProto>::GetInstance()
          ->GetForBrowserState(browser_state_b_.get()));
}
