// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/optimization_guide/model/optimization_guide_push_notification_client.h"

#import "base/base64.h"
#import "base/functional/callback_helpers.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/optimization_guide/core/hints_manager.h"
#import "components/optimization_guide/core/optimization_guide_features.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

constexpr char kHintKey[] = "https://www.foo.com";
constexpr char kPayloadValue[] = "value";
NSString* kSerializedPayloadKey = @"op";

}  // namespace

class MockDelegate
    : public optimization_guide::PushNotificationManager::Delegate {
 public:
  MOCK_METHOD(void,
              RemoveFetchedEntriesByHintKeys,
              (base::OnceClosure,
               optimization_guide::proto::KeyRepresentation,
               (const base::flat_set<std::string>&)),
              (override));
};

class MockOptimizationGuidePushNotificationClient
    : public OptimizationGuidePushNotificationClient {
 public:
  MockOptimizationGuidePushNotificationClient()
      : OptimizationGuidePushNotificationClient(
            PushNotificationClientId::kCommerce) {}

  // Override OptimizationGuidePushNotificationClient
  // Unused at OptimizationGuide level but included to make
  // MockOptimizationGuidePushNotificationClient non-abstract
  // and usable as a test class.
  NSArray<UNNotificationCategory*>* RegisterActionableNotifications() override {
    return @[];
  }
  void HandleNotificationInteraction(
      UNNotificationResponse* notification) override {}
  void OnSceneActiveForegroundBrowserReady() override {}
};

class OptimizationGuidePushNotificationClientTest : public PlatformTest {
 public:
  OptimizationGuidePushNotificationClientTest() {}
  ~OptimizationGuidePushNotificationClientTest() override = default;

  void SetUp() override {
    PlatformTest::SetUp();
    scoped_feature_list_.InitWithFeatures(
        {optimization_guide::features::kPushNotifications}, {});
    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        OptimizationGuideServiceFactory::GetInstance(),
        OptimizationGuideServiceFactory::GetDefaultFactory());
    browser_state_ = builder.Build();
  }

  ChromeBrowserState* GetBrowserState() { return browser_state_.get(); }

  void SetLastUsedChromeBrowserStateForTesting(
      MockOptimizationGuidePushNotificationClient& push_notification_client) {
    push_notification_client.SetLastUsedChromeBrowserStateForTesting(
        browser_state_.get());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  OptimizationGuideService* optimization_guide_service_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
};

TEST_F(OptimizationGuidePushNotificationClientTest, TestParsing) {
  optimization_guide::proto::Any any;
  optimization_guide::proto::HintNotificationPayload hint_notification_payload;
  hint_notification_payload.set_hint_key(kHintKey);
  hint_notification_payload.set_optimization_type(
      optimization_guide::proto::NOSCRIPT);
  hint_notification_payload.set_key_representation(
      optimization_guide::proto::HOST);
  optimization_guide::proto::Any* payload =
      hint_notification_payload.mutable_payload();
  payload->set_type_url(kHintKey);
  payload->set_value(kPayloadValue);

  std::string serialized_hint_notification_payload;
  hint_notification_payload.SerializeToString(
      &serialized_hint_notification_payload);
  any.set_value(serialized_hint_notification_payload.c_str());

  std::string serialized_any;
  any.SerializeToString(&serialized_any);
  std::string serialized_any_escaped;
  base::Base64Encode(serialized_any, &serialized_any_escaped);

  std::unique_ptr<optimization_guide::proto::HintNotificationPayload> parsed =
      OptimizationGuidePushNotificationClient::ParseHintNotificationPayload(
          base::SysUTF8ToNSString(serialized_any_escaped));
  EXPECT_EQ(kHintKey, parsed->hint_key());
  EXPECT_EQ(optimization_guide::proto::NOSCRIPT, parsed->optimization_type());
  EXPECT_EQ(optimization_guide::proto::HOST, parsed->key_representation());
  EXPECT_EQ(kHintKey, parsed->payload().type_url());
  EXPECT_EQ(kPayloadValue, parsed->payload().value());
}

TEST_F(OptimizationGuidePushNotificationClientTest,
       TestHintKeyRemovedUponNotification) {
  MockDelegate mock_delegate;
  OptimizationGuideService* optimization_guide_service =
      OptimizationGuideServiceFactory::GetForBrowserState(GetBrowserState());
  optimization_guide_service->GetHintsManager()
      ->push_notification_manager()
      ->SetDelegate(&mock_delegate);

  optimization_guide::proto::Any any;
  optimization_guide::proto::HintNotificationPayload hint_notification_payload;
  hint_notification_payload.set_hint_key(kHintKey);
  hint_notification_payload.set_key_representation(
      optimization_guide::proto::HOST);
  hint_notification_payload.set_optimization_type(
      optimization_guide::proto::NOSCRIPT);

  optimization_guide::proto::Any* payload =
      hint_notification_payload.mutable_payload();
  payload->set_type_url(kHintKey);
  payload->set_value(kPayloadValue);

  std::string serialized_hint_notification_payload;
  hint_notification_payload.SerializeToString(
      &serialized_hint_notification_payload);
  any.set_value(serialized_hint_notification_payload.c_str());

  std::string serialized_any;
  any.SerializeToString(&serialized_any);
  std::string serialized_any_escaped;
  base::Base64Encode(serialized_any, &serialized_any_escaped);

  NSDictionary* dict = @{
    kSerializedPayloadKey : base::SysUTF8ToNSString(serialized_any_escaped)
  };

  MockOptimizationGuidePushNotificationClient push_notification_client;
  SetLastUsedChromeBrowserStateForTesting(push_notification_client);

  EXPECT_CALL(mock_delegate,
              RemoveFetchedEntriesByHintKeys(
                  testing::_, testing::Eq(optimization_guide::proto::HOST),
                  testing::ElementsAreArray({kHintKey})));
  push_notification_client.HandleNotificationReception(dict);
}
