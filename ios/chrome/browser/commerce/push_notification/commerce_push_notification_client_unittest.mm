// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/commerce/push_notification/commerce_push_notification_client.h"

#import "base/base64.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/task_environment.h"
#import "components/commerce/core/proto/price_tracking.pb.h"
#import "components/optimization_guide/proto/push_notification.pb.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/browser_list.h"
#import "ios/chrome/browser/main/browser_list_factory.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/url_loading/fake_url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/url_loading_notifier_browser_agent.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

constexpr char kHintKey[] = "https://www.merchant.com/price_drop_product";
NSString* kSerializedPayloadKey = @"op";
NSString* kVisitSiteActionId = @"visit_site";
NSString* kVisitSiteTitle = @"Visit Site";

}  // namespace

class CommercePushNotificationClientTest : public PlatformTest {
 public:
  CommercePushNotificationClientTest() {}
  ~CommercePushNotificationClientTest() override = default;

  void SetUp() override {
    PlatformTest::SetUp();
    TestChromeBrowserState::Builder test_browser_state_builder;
    chrome_browser_state_ = test_browser_state_builder.Build();
    browser_list_ =
        BrowserListFactory::GetForBrowserState(chrome_browser_state_.get());
    browser_ = std::make_unique<TestBrowser>(chrome_browser_state_.get());
    browser_list_->AddBrowser(browser_.get());
    UrlLoadingNotifierBrowserAgent::CreateForBrowser(browser_.get());
    FakeUrlLoadingBrowserAgent::InjectForBrowser(browser_.get());
    commerce_push_notification_client_.SetLastUsedChromeBrowserStateForTesting(
        chrome_browser_state_.get());
  }

  CommercePushNotificationClient* GetCommercePushNotificationClient() {
    return &commerce_push_notification_client_;
  }

  Browser* GetBrowser() { return browser_.get(); }

  void HandleNotificationInteraction(NSString* action_identifier,
                                     NSDictionary* user_info) {
    commerce_push_notification_client_.HandleNotificationInteraction(
        action_identifier, user_info);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  CommercePushNotificationClient commerce_push_notification_client_;
  std::unique_ptr<Browser> browser_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  BrowserList* browser_list_;
};

TEST_F(CommercePushNotificationClientTest, TestNotificationInteraction) {
  // Serialized PriceDropNotificationPayload
  commerce::PriceDropNotificationPayload price_drop_notification;
  price_drop_notification.set_destination_url(kHintKey);
  std::string serialized_price_drop_notification;
  price_drop_notification.SerializeToString(
      &serialized_price_drop_notification);

  // Serialized HintNotificationPayload with PriceDropNotificationPayload
  // injected.
  optimization_guide::proto::HintNotificationPayload hint_notification_payload;
  hint_notification_payload.set_hint_key(kHintKey);
  hint_notification_payload.set_optimization_type(
      optimization_guide::proto::PRICE_TRACKING);
  hint_notification_payload.set_key_representation(
      optimization_guide::proto::HOST);
  optimization_guide::proto::Any* payload =
      hint_notification_payload.mutable_payload();
  payload->set_type_url(kHintKey);
  payload->set_value(serialized_price_drop_notification.c_str());
  std::string serialized_hint_notification_payload;
  hint_notification_payload.SerializeToString(
      &serialized_hint_notification_payload);

  // Serialized Any with HintNotificationPayload injected
  optimization_guide::proto::Any any;
  any.set_value(serialized_hint_notification_payload.c_str());
  std::string serialized_any;
  any.SerializeToString(&serialized_any);

  // Base 64 encoding
  std::string serialized_any_escaped;
  base::Base64Encode(serialized_any, &serialized_any_escaped);

  NSDictionary* user_info = @{
    kSerializedPayloadKey : base::SysUTF8ToNSString(serialized_any_escaped)
  };

  // Simulate user clicking 'visit site'.
  HandleNotificationInteraction(kVisitSiteActionId, user_info);

  // Check PriceDropNotification Destination URL loaded.
  FakeUrlLoadingBrowserAgent* url_loader =
      FakeUrlLoadingBrowserAgent::FromUrlLoadingBrowserAgent(
          UrlLoadingBrowserAgent::FromBrowser(GetBrowser()));
  EXPECT_EQ(price_drop_notification.destination_url(),
            url_loader->last_params.web_params.url);
}

TEST_F(CommercePushNotificationClientTest, TestActionableNotifications) {
  NSArray<UNNotificationCategory*>* actionable_notifications =
      GetCommercePushNotificationClient()->RegisterActionableNotifications();
  EXPECT_EQ(1u, [actionable_notifications count]);
  UNNotificationCategory* notification_category = actionable_notifications[0];
  EXPECT_TRUE([notification_category.actions[0].identifier
      isEqualToString:kVisitSiteActionId]);
  EXPECT_TRUE(
      [notification_category.actions[0].title isEqualToString:kVisitSiteTitle]);
}
