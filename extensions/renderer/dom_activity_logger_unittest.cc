// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/dom_activity_logger.h"

#include "base/command_line.h"
#include "extensions/common/dom_action_types.h"
#include "extensions/common/mojom/renderer_host.mojom.h"
#include "extensions/renderer/extensions_renderer_client.h"
#include "extensions/renderer/policy_activity_log_filter.h"
#include "extensions/renderer/test_extensions_renderer_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "url/gurl.h"

namespace extensions {

class MockPolicyActivityLogFilter : public PolicyActivityLogFilter {
 public:
  MOCK_METHOD(bool,
              IsHighRiskEvent,
              (const ExtensionId&,
               DomActionType::Type,
               const std::string&,
               const base::ListValue&,
               const GURL&),
              (override));
};

class MockRendererHost : public mojom::RendererHost {
 public:
  MockRendererHost() = default;
  ~MockRendererHost() override = default;

  MOCK_METHOD(void,
              AddAPIActionToActivityLog,
              (const std::optional<ExtensionId>&,
               const std::string&,
               base::ListValue,
               const std::string&),
              (override));
  MOCK_METHOD(void,
              AddEventToActivityLog,
              (const std::optional<ExtensionId>&,
               const std::string&,
               base::ListValue,
               const std::string&),
              (override));
  MOCK_METHOD(void,
              AddDOMActionToActivityLog,
              (const ExtensionId&,
               const std::string&,
               base::ListValue,
               const GURL&,
               const std::u16string&,
               int32_t),
              (override));
  MOCK_METHOD(void,
              GetMessageBundle,
              (const ExtensionId&, GetMessageBundleCallback),
              (override));
};

class MockExtensionsRendererClient : public TestExtensionsRendererClient {
 public:
  MockExtensionsRendererClient() = default;

  bool IsActivityLoggingEnabled() const override {
    return activity_logging_enabled_;
  }

  void set_activity_logging_enabled(bool enabled) {
    activity_logging_enabled_ = enabled;
  }

  bool IsPolicyActivityLoggingEnabled() const override {
    return policy_activity_logging_enabled_;
  }

  void set_policy_activity_logging_enabled(bool enabled) {
    policy_activity_logging_enabled_ = enabled;
  }

  PolicyActivityLogFilter* GetPolicyActivityLogFilter() override {
    return filter_;
  }

  void set_filter(PolicyActivityLogFilter* filter) { filter_ = filter; }

 private:
  bool activity_logging_enabled_ = false;
  bool policy_activity_logging_enabled_ = false;
  raw_ptr<PolicyActivityLogFilter> filter_ = nullptr;
};

// A test subclass to inject a mock RendererHost and expose Log methods.
class TestDOMActivityLogger : public DOMActivityLogger {
 public:
  using DOMActivityLogger::DOMActivityLogger;
  void set_renderer_host(mojom::RendererHost* host) { host_ = host; }

  // Expose for testing.
  using DOMActivityLogger::LogGetter;

 protected:
  mojom::RendererHost* GetRendererHost(
      v8::Local<v8::Context> context) override {
    return host_;
  }

 private:
  raw_ptr<mojom::RendererHost> host_ = nullptr;
};

class DOMActivityLoggerTest : public testing::Test {
 public:
  DOMActivityLoggerTest() = default;

 protected:
  void SetUp() override {
    client_ = std::make_unique<MockExtensionsRendererClient>();
    logger_ = std::make_unique<TestDOMActivityLogger>("test_extension");
  }

  void TearDown() override {
    logger_.reset();
    client_.reset();
  }

  MockExtensionsRendererClient* client() { return client_.get(); }
  TestDOMActivityLogger* logger() { return logger_.get(); }

 private:
  std::unique_ptr<MockExtensionsRendererClient> client_;
  std::unique_ptr<TestDOMActivityLogger> logger_;
};

TEST_F(DOMActivityLoggerTest, LogGetter_ActivityLoggingEnabledLogsEverything) {
  client()->set_activity_logging_enabled(true);
  client()->set_policy_activity_logging_enabled(false);

  testing::StrictMock<MockRendererHost> renderer_host;
  logger()->set_renderer_host(&renderer_host);

  EXPECT_CALL(
      renderer_host,
      AddDOMActionToActivityLog(
          testing::Eq("test_extension"), testing::Eq("api_name"), testing::_,
          testing::Eq(GURL("https://example.com")), testing::_,
          testing::Eq(static_cast<int32_t>(DomActionType::GETTER))));

  v8::Local<v8::Context> dummy_context;
  logger()->LogGetter(nullptr, dummy_context,
                      blink::WebString::FromUTF8("api_name"),
                      blink::WebURL(GURL("https://example.com")),
                      blink::WebString::FromUTF8("title"));
}

TEST_F(DOMActivityLoggerTest, LogGetter_PolicyDrivenCheck) {
  client()->set_activity_logging_enabled(false);
  client()->set_policy_activity_logging_enabled(true);

  testing::StrictMock<MockPolicyActivityLogFilter> filter;
  client()->set_filter(&filter);

  testing::StrictMock<MockRendererHost> renderer_host;
  logger()->set_renderer_host(&renderer_host);

  v8::Local<v8::Context> dummy_context;
  const GURL kUrl("https://example.com");

  // 1. Benign event -> No IPC.
  EXPECT_CALL(filter, IsHighRiskEvent(testing::Eq("test_extension"),
                                      testing::Eq(DomActionType::GETTER),
                                      testing::Eq("benign_api"), testing::_,
                                      testing::Eq(kUrl)))
      .WillOnce(testing::Return(false));

  logger()->LogGetter(nullptr, dummy_context,
                      blink::WebString::FromUTF8("benign_api"),
                      blink::WebURL(kUrl), blink::WebString::FromUTF8("title"));

  // 2. Risky event -> IPC sent.
  EXPECT_CALL(filter, IsHighRiskEvent(testing::Eq("test_extension"),
                                      testing::Eq(DomActionType::GETTER),
                                      testing::Eq("risky_api"), testing::_,
                                      testing::Eq(kUrl)))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(renderer_host,
              AddDOMActionToActivityLog(
                  testing::Eq("test_extension"), testing::Eq("risky_api"),
                  testing::_, testing::Eq(kUrl), testing::_,
                  testing::Eq(static_cast<int32_t>(DomActionType::GETTER))));

  logger()->LogGetter(nullptr, dummy_context,
                      blink::WebString::FromUTF8("risky_api"),
                      blink::WebURL(kUrl), blink::WebString::FromUTF8("title"));
}

}  // namespace extensions
