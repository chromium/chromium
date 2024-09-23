// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/notification/notification_client.h"

#include <memory>
#include <optional>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
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
constexpr char kTestOsVersion[] = "15.1";
constexpr char kTestLocale[] = "zh-CN";

class MockJsonFetcher : public JsonFetcher {
 public:
  // GMock doesn't work with rvalue parameters. This works around it.
  MOCK_CONST_METHOD1(FetchJsonFile,
                     std::optional<base::Value>(const std::string&));
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

base::Value::Dict CreateDefaultRule() {
  base::Value::Dict rule;
  rule.Set("target_platform", "IOS");
  rule.Set("version", "[75-)");
  rule.Set("message_id", "test_message");
  rule.Set("message_text", "message_text.json");
  rule.Set("link_text", "link_text.json");
  rule.Set("link_url", "https://example.com/some_link");
  rule.Set("percent", 100);
  rule.Set("allow_silence", true);
  return rule;
}

base::Value CreateDefaultTranslations(const std::string& text) {
  base::Value::Dict translations;
  translations.Set("en-US", "en-US:" + text);
  translations.Set("zh-CN", "zh-CN:" + text);
  return base::Value(std::move(translations));
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
  void Reset(bool should_ignore_dev_messages,
             const std::string& test_locale = kTestLocale) {
    auto fetcher = std::make_unique<MockJsonFetcher>();
    fetcher_ = fetcher.get();
    client_ = base::WrapUnique(new NotificationClient(
        std::move(fetcher), kTestPlatform, kTestVersion, kTestOsVersion,
        test_locale, should_ignore_dev_messages));
  }

  std::unique_ptr<NotificationClient> client_;
  raw_ptr<MockJsonFetcher> fetcher_;  // Owned by `client_`.
};

TEST_F(NotificationClientTest, NoRule) {
  base::Value::List rules;
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/rules.json"))
      .WillOnce(ReturnByMove(base::Value(std::move(rules))));

  base::MockCallback<NotificationClient::NotificationCallback> callback;
  EXPECT_CALL(callback, Run(NoMessage()));
  client_->GetNotification(kTestEmail, callback.Get());
}

TEST_F(NotificationClientTest, DefaultRule) {
  base::Value::List rules;
  rules.Append(CreateDefaultRule());
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/rules.json"))
      .WillOnce(ReturnByMove(base::Value(std::move(rules))));
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/message_text.json"))
      .WillOnce(ReturnByMove(CreateDefaultTranslations("message")));
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/link_text.json"))
      .WillOnce(ReturnByMove(CreateDefaultTranslations("link")));

  base::MockCallback<NotificationClient::NotificationCallback> callback;
  EXPECT_CALL(callback, Run(MessageMatches(CreateDefaultNotification())));
  client_->GetNotification(kTestEmail, callback.Get());
}

TEST_F(NotificationClientTest, PlatformNotMatched) {
  base::Value::Dict rule = CreateDefaultRule();
  rule.Set("target_platform", "ANDROID");
  base::Value::List rules;
  rules.Append(std::move(rule));
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/rules.json"))
      .WillOnce(ReturnByMove(base::Value(std::move(rules))));

  base::MockCallback<NotificationClient::NotificationCallback> callback;
  EXPECT_CALL(callback, Run(NoMessage()));
  client_->GetNotification(kTestEmail, callback.Get());
}

TEST_F(NotificationClientTest, VersionNotMatched) {
  base::Value::Dict rule = CreateDefaultRule();
  rule.Set("version", "[77-)");
  base::Value::List rules;
  rules.Append(std::move(rule));
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/rules.json"))
      .WillOnce(ReturnByMove(base::Value(std::move(rules))));

  base::MockCallback<NotificationClient::NotificationCallback> callback;
  EXPECT_CALL(callback, Run(NoMessage()));
  client_->GetNotification(kTestEmail, callback.Get());
}

TEST_F(NotificationClientTest, OsVersionNotMatched) {
  base::Value::Dict rule = CreateDefaultRule();
  rule.Set("os_version", "(-15.1)");
  base::Value::List rules;
  rules.Append(std::move(rule));
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/rules.json"))
      .WillOnce(ReturnByMove(base::Value(std::move(rules))));

  base::MockCallback<NotificationClient::NotificationCallback> callback;
  EXPECT_CALL(callback, Run(NoMessage()));
  client_->GetNotification(kTestEmail, callback.Get());
}

TEST_F(NotificationClientTest, OsVersionMatched) {
  base::Value::Dict rule = CreateDefaultRule();
  rule.Set("os_version", "[15-)");
  base::Value::List rules;
  rules.Append(std::move(rule));
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/rules.json"))
      .WillOnce(ReturnByMove(base::Value(std::move(rules))));
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/message_text.json"))
      .WillOnce(ReturnByMove(CreateDefaultTranslations("message")));
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/link_text.json"))
      .WillOnce(ReturnByMove(CreateDefaultTranslations("link")));

  base::MockCallback<NotificationClient::NotificationCallback> callback;
  EXPECT_CALL(callback, Run(MessageMatches(CreateDefaultNotification())));
  client_->GetNotification(kTestEmail, callback.Get());
}

TEST_F(NotificationClientTest, UserNotSelected) {
  base::Value::Dict rule = CreateDefaultRule();
  rule.Set("percent", 0);
  base::Value::List rules;
  rules.Append(std::move(rule));
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/rules.json"))
      .WillOnce(ReturnByMove(base::Value(std::move(rules))));

  base::MockCallback<NotificationClient::NotificationCallback> callback;
  EXPECT_CALL(callback, Run(NoMessage()));
  client_->GetNotification(kTestEmail, callback.Get());
}

TEST_F(NotificationClientTest, SecondRuleMatches) {
  base::Value::List rules;
  base::Value::Dict rule_1 = CreateDefaultRule();
  rule_1.Set("target_platform", "ANDROID");
  rule_1.Set("message_text", "message_text_1.json");
  rules.Append(std::move(rule_1));
  rules.Append(CreateDefaultRule());
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/rules.json"))
      .WillOnce(ReturnByMove(base::Value(std::move(rules))));
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/message_text.json"))
      .WillOnce(ReturnByMove(CreateDefaultTranslations("message")));
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/link_text.json"))
      .WillOnce(ReturnByMove(CreateDefaultTranslations("link")));

  base::MockCallback<NotificationClient::NotificationCallback> callback;
  EXPECT_CALL(callback, Run(MessageMatches(CreateDefaultNotification())));
  client_->GetNotification(kTestEmail, callback.Get());
}

TEST_F(NotificationClientTest, MultipleMatchingRules_FirstRuleSelected) {
  base::Value::List rules;
  rules.Append(CreateDefaultRule());
  base::Value::Dict rule_2 = CreateDefaultRule();
  rule_2.Set("message_text", "message_text_2.json");
  rules.Append(std::move(rule_2));
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/rules.json"))
      .WillOnce(ReturnByMove(base::Value(std::move(rules))));
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/message_text.json"))
      .WillOnce(ReturnByMove(CreateDefaultTranslations("message")));
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/link_text.json"))
      .WillOnce(ReturnByMove(CreateDefaultTranslations("link")));

  base::MockCallback<NotificationClient::NotificationCallback> callback;
  EXPECT_CALL(callback, Run(MessageMatches(CreateDefaultNotification())));
  client_->GetNotification(kTestEmail, callback.Get());
}

TEST_F(NotificationClientTest, TextFilesNotFound) {
  base::Value::List rules;
  rules.Append(CreateDefaultRule());
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/rules.json"))
      .WillOnce(ReturnByMove(base::Value(std::move(rules))));

  base::Value translation = CreateDefaultTranslations("message");

  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/message_text.json"))
      .WillOnce(ReturnByMove(std::nullopt));
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/link_text.json"))
      .WillOnce(ReturnByMove(std::nullopt));

  base::MockCallback<NotificationClient::NotificationCallback> callback;
  EXPECT_CALL(callback, Run(NoMessage()));
  client_->GetNotification(kTestEmail, callback.Get());
}

TEST_F(NotificationClientTest, TranslationNotFound_FallbackToGenericLanguage) {
  base::Value::List rules;
  rules.Append(CreateDefaultRule());
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/rules.json"))
      .WillOnce(ReturnByMove(base::Value(std::move(rules))));

  base::Value translations = CreateDefaultTranslations("message");
  translations.GetDict().Remove("zh-CN");
  translations.GetDict().Set("zh", "zh:message");
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/message_text.json"))
      .WillOnce(ReturnByMove(std::move(translations)));
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/link_text.json"))
      .WillOnce(ReturnByMove(CreateDefaultTranslations("link")));

  NotificationMessage notification = CreateDefaultNotification();
  notification.message_text = "zh:message";
  base::MockCallback<NotificationClient::NotificationCallback> callback;
  EXPECT_CALL(callback, Run(MessageMatches(notification)));
  client_->GetNotification(kTestEmail, callback.Get());
}

TEST_F(NotificationClientTest, TranslationNotFound_FallbackToEnglish) {
  base::Value::List rules;
  rules.Append(CreateDefaultRule());
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/rules.json"))
      .WillOnce(ReturnByMove(base::Value(std::move(rules))));

  base::Value translations = CreateDefaultTranslations("message");
  translations.GetDict().Remove("zh-CN");
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

TEST_F(NotificationClientTest,
       GenericLanguageTranslationNotFound_FallbackToEnglish) {
  Reset(/* should_ignore_dev_messages= */ false, /* test_locale= */ "es");

  base::Value::List rules;
  rules.Append(CreateDefaultRule());
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/rules.json"))
      .WillOnce(ReturnByMove(base::Value(std::move(rules))));

  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/message_text.json"))
      .WillOnce(ReturnByMove(CreateDefaultTranslations("message")));
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/link_text.json"))
      .WillOnce(ReturnByMove(CreateDefaultTranslations("link")));

  NotificationMessage notification = CreateDefaultNotification();
  notification.message_text = "en-US:message";
  notification.link_text = "en-US:link";
  base::MockCallback<NotificationClient::NotificationCallback> callback;
  EXPECT_CALL(callback, Run(MessageMatches(notification)));
  client_->GetNotification(kTestEmail, callback.Get());
}

TEST_F(NotificationClientTest, NoAvailableTranslation) {
  base::Value::List rules;
  rules.Append(CreateDefaultRule());
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/rules.json"))
      .WillOnce(ReturnByMove(base::Value(std::move(rules))));

  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/message_text.json"))
      .WillOnce(ReturnByMove(base::Value(base::Value::Type::DICT)));
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/link_text.json"))
      .WillOnce(ReturnByMove(base::Value(base::Value::Type::DICT)));

  base::MockCallback<NotificationClient::NotificationCallback> callback;
  EXPECT_CALL(callback, Run(NoMessage()));
  client_->GetNotification(kTestEmail, callback.Get());
}

TEST_F(NotificationClientTest, ReleaseBuildsIgnoreDevMessages) {
  base::Value::List rules;
  base::Value::Dict rule = CreateDefaultRule();
  rule.Set("dev_mode", true);
  rules.Append(std::move(rule));
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/rules.json"))
      .WillOnce(ReturnByMove(base::Value(std::move(rules))));

  base::MockCallback<NotificationClient::NotificationCallback> callback;
  EXPECT_CALL(callback, Run(NoMessage()));
  client_->GetNotification(kTestEmail, callback.Get());
}

TEST_F(NotificationClientTest, ReleaseBuildsDontIgnoreDevMessages) {
  base::Value::List rules;
  base::Value::Dict rule = CreateDefaultRule();
  rule.Set("dev_mode", false);
  rules.Append(std::move(rule));
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/rules.json"))
      .WillOnce(ReturnByMove(base::Value(std::move(rules))));
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

  base::Value::List rules;
  base::Value::Dict rule = CreateDefaultRule();
  rule.Set("dev_mode", true);
  rules.Append(std::move(rule));
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/rules.json"))
      .WillOnce(ReturnByMove(base::Value(std::move(rules))));
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/message_text.json"))
      .WillOnce(ReturnByMove(CreateDefaultTranslations("message")));
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/link_text.json"))
      .WillOnce(ReturnByMove(CreateDefaultTranslations("link")));

  base::MockCallback<NotificationClient::NotificationCallback> callback;
  EXPECT_CALL(callback, Run(MessageMatches(CreateDefaultNotification())));
  client_->GetNotification(kTestEmail, callback.Get());
}

TEST_F(NotificationClientTest, AllowSilenceNotSet_DefaultToFalse) {
  base::Value::List rules;
  base::Value::Dict rule = CreateDefaultRule();
  rule.Remove("allow_silence");
  rules.Append(std::move(rule));
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/rules.json"))
      .WillOnce(ReturnByMove(base::Value(std::move(rules))));
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

TEST_F(NotificationClientTest,
       EmptyUserEmailAndNot100PercentRollout_NoNotification) {
  base::Value::List rules;
  base::Value::Dict rule = CreateDefaultRule();
  rule.Set("percent", 99);
  rules.Append(std::move(rule));
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/rules.json"))
      .WillOnce(ReturnByMove(base::Value(std::move(rules))));

  base::MockCallback<NotificationClient::NotificationCallback> callback;
  EXPECT_CALL(callback, Run(NoMessage()));
  client_->GetNotification(/* user_email= */ "", callback.Get());
}

TEST_F(NotificationClientTest,
       EmptyUserEmailAnd100PercentRollout_ReturnsNotification) {
  base::Value::List rules;
  base::Value::Dict rule = CreateDefaultRule();
  rule.Set("percent", 100);
  rules.Append(std::move(rule));
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/rules.json"))
      .WillOnce(ReturnByMove(base::Value(std::move(rules))));
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/message_text.json"))
      .WillOnce(ReturnByMove(CreateDefaultTranslations("message")));
  EXPECT_CALL(*fetcher_, FetchJsonFile("notification/link_text.json"))
      .WillOnce(ReturnByMove(CreateDefaultTranslations("link")));

  base::MockCallback<NotificationClient::NotificationCallback> callback;
  EXPECT_CALL(callback, Run(MessageMatches(CreateDefaultNotification())));
  client_->GetNotification(/* user_email= */ "", callback.Get());
}

}  // namespace remoting
