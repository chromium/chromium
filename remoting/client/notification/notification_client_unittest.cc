// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/notification/notification_client.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/test/mock_callback.h"
#include "base/values.h"
#include "remoting/client/notification/json_fetcher.h"
#include "remoting/client/notification/notification_message.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

using ::testing::ByMove;
using ::testing::Return;

constexpr char kTestEmail[] = "test@example.com";
constexpr char kTestPlatform[] = "IOS";
constexpr char kTestVersion[] = "76.0.3809.13";
constexpr char kTestLocale[] = "zh-CN";

class MockJsonFetcher : public JsonFetcher {
 public:
  // GMock doesn't work with rvalue parameters. This works around it.
  MOCK_CONST_METHOD1(FetchJsonFile,
                     base::Optional<base::Value>(const std::string&));
  void FetchJsonFile(const std::string& relative_path,
                     FetchJsonFileCallback done,
                     const net::NetworkTrafficAnnotationTag&) override {
    auto value_opt = FetchJsonFile(relative_path);
    std::move(done).Run(std::move(value_opt));
  }
};

MATCHER(NoMessage, "") {
  return !arg.has_value();
}

MATCHER_P(MessageMatches, expected, "") {
  return arg->message_id == expected.message_id &&
         arg->message_text == expected.message_text &&
         arg->link_text == expected.link_text &&
         arg->link_url == expected.link_url;
}

template <typename T>
decltype(auto) ReturnByMove(T t) {
  return Return(ByMove(std::move(t)));
}

base::Value CreateDefaultRule() {
  base::Value rule(base::Value::Type::DICTIONARY);
  rule.SetStringKey("target_platform", "IOS");
  rule.SetStringKey("version", "[75-)");
  rule.SetStringKey("message_id", "test_message");
  rule.SetStringKey("message_text", "message_text.json");
  rule.SetStringKey("link_text", "link_text.json");
  rule.SetStringKey("link_url", "https://example.com/some_link");
  rule.SetIntKey("percent", 100);
  rule.SetBoolKey("allow_silence", true);
  return rule;
}

base::Value CreateDefaultTranslations(const std::string& text) {
  base::Value translations(base::Value::Type::DICTIONARY);
  translations.SetStringKey("en-US", "en-US:" + text);
  translations.SetStringKey("zh-CN", "zh-CN:" + text);
  return translations;
}

NotificationMessage CreateDefaultNotification() {
  NotificationMessage message;
  message.message_id = "test_message";
  message.message_text = "zh-CN:message";
  message.link_text = "zh-CN:link";
  message.link_url = "https://example.com/some_link";
  message.allow_silence = true;
  return message;
}

}  // namespace

class NotificationClientTest : public ::testing::Test {
 public:
  NotificationClientTest() { Reset(true); }

  ~NotificationClientTest() override = default;

 protected:
  void Reset(bool should_ignore_dev_messages) {
    auto fetcher = std::make_unique<MockJsonFetcher>();
    fetcher_ = fetcher.get();
    client_ = base::WrapUnique(
        new NotificationClient(std::move(fetcher), kTestPlatform, kTestVersion,
                               kTestLocale, should_ignore_dev_messages));
  }

  MockJsonFetcher* fetcher_;
  std::unique_ptr<NotificationClient> client_;
};

TEST_F(NotificationClientTest, NoRule) {
  base::Value rules(base::Value::Type::LIST);
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/rules.json"))
      .WillOnce(ReturnByMove(std::move(rules)));

  base::MockCallback<NotificationClient::NotificationCallback> callback;
  EXPECT_CALL(callback, Run(NoMessage()));
  client_->GetNotification(kTestEmail, callback.Get());
}

TEST_F(NotificationClientTest, DefaultRule) {
  base::Value rules(base::Value::Type::LIST);
  rules.Append(CreateDefaultRule());
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/rules.json"))
      .WillOnce(ReturnByMove(std::move(rules)));
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/message_text.json"))
      .WillOnce(ReturnByMove(CreateDefaultTranslations("message")));
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/link_text.json"))
      .WillOnce(ReturnByMove(CreateDefaultTranslations("link")));

  base::MockCallback<NotificationClient::NotificationCallback> callback;
  EXPECT_CALL(callback, Run(MessageMatches(CreateDefaultNotification())));
  client_->GetNotification(kTestEmail, callback.Get());
}

TEST_F(NotificationClientTest, PlatformNotMatched) {
  base::Value rule = CreateDefaultRule();
  rule.SetStringKey("target_platform", "ANDROID");
  base::Value rules(base::Value::Type::LIST);
  rules.Append(std::move(rule));
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/rules.json"))
      .WillOnce(ReturnByMove(std::move(rules)));

  base::MockCallback<NotificationClient::NotificationCallback> callback;
  EXPECT_CALL(callback, Run(NoMessage()));
  client_->GetNotification(kTestEmail, callback.Get());
}

TEST_F(NotificationClientTest, VersionNotMatched) {
  base::Value rule = CreateDefaultRule();
  rule.SetStringKey("version", "[77-)");
  base::Value rules(base::Value::Type::LIST);
  rules.Append(std::move(rule));
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/rules.json"))
      .WillOnce(ReturnByMove(std::move(rules)));

  base::MockCallback<NotificationClient::NotificationCallback> callback;
  EXPECT_CALL(callback, Run(NoMessage()));
  client_->GetNotification(kTestEmail, callback.Get());
}

TEST_F(NotificationClientTest, UserNotSelected) {
  base::Value rule = CreateDefaultRule();
  rule.SetIntKey("percent", 0);
  base::Value rules(base::Value::Type::LIST);
  rules.Append(std::move(rule));
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/rules.json"))
      .WillOnce(ReturnByMove(std::move(rules)));

  base::MockCallback<NotificationClient::NotificationCallback> callback;
  EXPECT_CALL(callback, Run(NoMessage()));
  client_->GetNotification(kTestEmail, callback.Get());
}

TEST_F(NotificationClientTest, SecondRuleMatches) {
  base::Value rules(base::Value::Type::LIST);
  base::Value rule_1 = CreateDefaultRule();
  rule_1.SetStringKey("target_platform", "ANDROID");
  rule_1.SetStringKey("message_text", "message_text_1.json");
  rules.Append(std::move(rule_1));
  rules.Append(CreateDefaultRule());
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/rules.json"))
      .WillOnce(ReturnByMove(std::move(rules)));
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/message_text.json"))
      .WillOnce(ReturnByMove(CreateDefaultTranslations("message")));
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/link_text.json"))
      .WillOnce(ReturnByMove(CreateDefaultTranslations("link")));

  base::MockCallback<NotificationClient::NotificationCallback> callback;
  EXPECT_CALL(callback, Run(MessageMatches(CreateDefaultNotification())));
  client_->GetNotification(kTestEmail, callback.Get());
}

TEST_F(NotificationClientTest, MultipleMatchingRules_FirstRuleSelected) {
  base::Value rules(base::Value::Type::LIST);
  rules.Append(CreateDefaultRule());
  base::Value rule_2 = CreateDefaultRule();
  rule_2.SetStringKey("message_text", "message_text_2.json");
  rules.Append(std::move(rule_2));
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/rules.json"))
      .WillOnce(ReturnByMove(std::move(rules)));
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/message_text.json"))
      .WillOnce(ReturnByMove(CreateDefaultTranslations("message")));
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/link_text.json"))
      .WillOnce(ReturnByMove(CreateDefaultTranslations("link")));

  base::MockCallback<NotificationClient::NotificationCallback> callback;
  EXPECT_CALL(callback, Run(MessageMatches(CreateDefaultNotification())));
  client_->GetNotification(kTestEmail, callback.Get());
}

TEST_F(NotificationClientTest, TextFilesNotFound) {
  base::Value rules(base::Value::Type::LIST);
  rules.Append(CreateDefaultRule());
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/rules.json"))
      .WillOnce(ReturnByMove(std::move(rules)));

  base::Value translation = CreateDefaultTranslations("message");

  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/message_text.json"))
      .WillOnce(ReturnByMove(base::nullopt));
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/link_text.json"))
      .WillOnce(ReturnByMove(base::nullopt));

  base::MockCallback<NotificationClient::NotificationCallback> callback;
  EXPECT_CALL(callback, Run(NoMessage()));
  client_->GetNotification(kTestEmail, callback.Get());
}

TEST_F(NotificationClientTest, TranslationNotFound_FallbackToEnglish) {
  base::Value rules(base::Value::Type::LIST);
  rules.Append(CreateDefaultRule());
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/rules.json"))
      .WillOnce(ReturnByMove(std::move(rules)));

  base::Value translations = CreateDefaultTranslations("message");
  translations.RemoveKey("zh-CN");
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/message_text.json"))
      .WillOnce(ReturnByMove(std::move(translations)));
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/link_text.json"))
      .WillOnce(ReturnByMove(CreateDefaultTranslations("link")));

  NotificationMessage notification = CreateDefaultNotification();
  notification.message_text = "en-US:message";
  base::MockCallback<NotificationClient::NotificationCallback> callback;
  EXPECT_CALL(callback, Run(MessageMatches(notification)));
  client_->GetNotification(kTestEmail, callback.Get());
}

TEST_F(NotificationClientTest, NoAvailableTranslation) {
  base::Value rules(base::Value::Type::LIST);
  rules.Append(CreateDefaultRule());
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/rules.json"))
      .WillOnce(ReturnByMove(std::move(rules)));

  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/message_text.json"))
      .WillOnce(ReturnByMove(base::Value(base::Value::Type::DICTIONARY)));
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/link_text.json"))
      .WillOnce(ReturnByMove(base::Value(base::Value::Type::DICTIONARY)));

  base::MockCallback<NotificationClient::NotificationCallback> callback;
  EXPECT_CALL(callback, Run(NoMessage()));
  client_->GetNotification(kTestEmail, callback.Get());
}

TEST_F(NotificationClientTest, ReleaseBuildsIgnoreDevMessages) {
  base::Value rules(base::Value::Type::LIST);
  base::Value rule = CreateDefaultRule();
  rule.SetBoolKey("dev_mode", true);
  rules.Append(std::move(rule));
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/rules.json"))
      .WillOnce(ReturnByMove(std::move(rules)));

  base::MockCallback<NotificationClient::NotificationCallback> callback;
  EXPECT_CALL(callback, Run(NoMessage()));
  client_->GetNotification(kTestEmail, callback.Get());
}

TEST_F(NotificationClientTest, ReleaseBuildsDontIgnoreDevMessages) {
  base::Value rules(base::Value::Type::LIST);
  base::Value rule = CreateDefaultRule();
  rule.SetBoolKey("dev_mode", false);
  rules.Append(std::move(rule));
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/rules.json"))
      .WillOnce(ReturnByMove(std::move(rules)));
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/message_text.json"))
      .WillOnce(ReturnByMove(CreateDefaultTranslations("message")));
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/link_text.json"))
      .WillOnce(ReturnByMove(CreateDefaultTranslations("link")));

  base::MockCallback<NotificationClient::NotificationCallback> callback;
  EXPECT_CALL(callback, Run(MessageMatches(CreateDefaultNotification())));
  client_->GetNotification(kTestEmail, callback.Get());
}

TEST_F(NotificationClientTest, DebugBuildsDontIgnoreDevMessages) {
  Reset(/* should_ignore_dev_messages */ false);

  base::Value rules(base::Value::Type::LIST);
  base::Value rule = CreateDefaultRule();
  rule.SetBoolKey("dev_mode", true);
  rules.Append(std::move(rule));
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/rules.json"))
      .WillOnce(ReturnByMove(std::move(rules)));
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/message_text.json"))
      .WillOnce(ReturnByMove(CreateDefaultTranslations("message")));
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/link_text.json"))
      .WillOnce(ReturnByMove(CreateDefaultTranslations("link")));

  base::MockCallback<NotificationClient::NotificationCallback> callback;
  EXPECT_CALL(callback, Run(MessageMatches(CreateDefaultNotification())));
  client_->GetNotification(kTestEmail, callback.Get());
}

TEST_F(NotificationClientTest, AllowSilenceNotSet_DefaultToFalse) {
  base::Value rules(base::Value::Type::LIST);
  base::Value rule = CreateDefaultRule();
  rule.RemoveKey("allow_silence");
  rules.Append(std::move(rule));
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/rules.json"))
      .WillOnce(ReturnByMove(std::move(rules)));
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/message_text.json"))
      .WillOnce(ReturnByMove(CreateDefaultTranslations("message")));
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/link_text.json"))
      .WillOnce(ReturnByMove(CreateDefaultTranslations("link")));

  NotificationMessage notification = CreateDefaultNotification();
  notification.allow_silence = false;
  base::MockCallback<NotificationClient::NotificationCallback> callback;
  EXPECT_CALL(callback, Run(MessageMatches(notification)));
  client_->GetNotification(kTestEmail, callback.Get());
}

}  // namespace remoting