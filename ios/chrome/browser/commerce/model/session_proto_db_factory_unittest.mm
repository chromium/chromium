// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/commerce/model/session_proto_db_factory.h"

#import "components/commerce/core/proto/commerce_subscription_db_content.pb.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

class SessionProtoDBFactoryTest : public PlatformTest {
 public:
  SessionProtoDBFactoryTest() {}

  void SetUp() override {
    TestProfileIOS::Builder builder_a;
    profile_a_ = std::move(builder_a).Build();
    TestProfileIOS::Builder builder_b;
    profile_b_ = std::move(builder_b).Build();
  }

 protected:
  std::unique_ptr<TestProfileIOS> profile_a_;
  std::unique_ptr<TestProfileIOS> profile_b_;
  web::WebTaskEnvironment task_environment_;
};

TEST_F(SessionProtoDBFactoryTest, TestIncognito) {
  EXPECT_EQ(
      nullptr,
      SessionProtoDBFactory<commerce_subscription_db::
                                CommerceSubscriptionContentProto>::GetInstance()
          ->GetForProfile(profile_a_->GetOffTheRecordProfile()));
}

TEST_F(SessionProtoDBFactoryTest, TestNonIncognito) {
  EXPECT_NE(
      nullptr,
      SessionProtoDBFactory<commerce_subscription_db::
                                CommerceSubscriptionContentProto>::GetInstance()
          ->GetForProfile(profile_a_.get()));
}

TEST_F(SessionProtoDBFactoryTest, TestSameBrowserState) {
  EXPECT_EQ(
      SessionProtoDBFactory<commerce_subscription_db::
                                CommerceSubscriptionContentProto>::GetInstance()
          ->GetForProfile(profile_a_.get()),
      SessionProtoDBFactory<commerce_subscription_db::
                                CommerceSubscriptionContentProto>::GetInstance()
          ->GetForProfile(profile_a_.get()));
}

TEST_F(SessionProtoDBFactoryTest, TestDifferentBrowserState) {
  EXPECT_NE(
      SessionProtoDBFactory<commerce_subscription_db::
                                CommerceSubscriptionContentProto>::GetInstance()
          ->GetForProfile(profile_a_.get()),
      SessionProtoDBFactory<commerce_subscription_db::
                                CommerceSubscriptionContentProto>::GetInstance()
          ->GetForProfile(profile_b_.get()));
}
