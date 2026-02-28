// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_credential_element.h"

#include "services/network/public/cpp/permissions_policy/permissions_policy.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/console_message_storage.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

class HTMLCredentialElementTest : public PageTestBase {};

TEST_F(HTMLCredentialElementTest, TagName) {
  auto* element = MakeGarbageCollected<HTMLCredentialElement>(GetDocument());
  EXPECT_EQ(element->tagName(), "CREDENTIAL");
  EXPECT_EQ(element->localName(), "credential");
}

TEST_F(HTMLCredentialElementTest, GetFederatedRequestOptions_Success) {
  NavigateTo(KURL("https://example.com"));
  auto* element = MakeGarbageCollected<HTMLCredentialElement>(GetDocument());
  element->setAttribute(html_names::kTypeAttr, AtomicString("federated"));
  element->setAttribute(html_names::kConfigurlAttr,
                        AtomicString("https://idp.com/config.json"));
  element->setAttribute(html_names::kClientidAttr, AtomicString("client123"));

  auto options = element->GetFederatedRequestOptions();
  ASSERT_TRUE(options);
  EXPECT_EQ(options->config->config_url, "https://idp.com/config.json");
  EXPECT_EQ(options->config->client_id, "client123");
}

TEST_F(HTMLCredentialElementTest, GetFederatedRequestOptions_BlockedByCSP) {
  NavigateTo(KURL("https://example.com"));
  ExecutionContext* context = GetDocument().GetExecutionContext();

  // Set up CSP to block the IDP.
  context->GetContentSecurityPolicy()->AddPolicies(ParseContentSecurityPolicies(
      "connect-src https://allowed.com",
      network::mojom::blink::ContentSecurityPolicyType::kEnforce,
      network::mojom::blink::ContentSecurityPolicySource::kHTTP,
      *(context->GetSecurityOrigin())));

  ConsoleMessageStorage& storage = GetPage().GetConsoleMessageStorage();
  wtf_size_t initial_size = storage.size();

  auto* element = MakeGarbageCollected<HTMLCredentialElement>(GetDocument());
  GetDocument().body()->AppendChild(element);

  element->setAttribute(html_names::kTypeAttr, AtomicString("federated"));
  element->setAttribute(html_names::kConfigurlAttr,
                        AtomicString("https://blocked.com/config.json"));
  element->setAttribute(html_names::kClientidAttr, AtomicString("client123"));

  // Should return nullptr because of CSP violation.
  EXPECT_FALSE(element->GetFederatedRequestOptions());

  // Check that a console message was emitted immediately.
  ASSERT_EQ(storage.size(), initial_size + 1);
  ASSERT_GT(storage.size(), 0u);
  EXPECT_TRUE(storage.at(storage.size() - 1)
                  ->Message()
                  .contains("Refused to connect to"));

  // Ensure no additional message is emitted.
  EXPECT_EQ(storage.size(), initial_size + 1);
}

TEST_F(HTMLCredentialElementTest, ParamsAttributeValidation) {
  NavigateTo(KURL("https://example.com"));
  auto* element = MakeGarbageCollected<HTMLCredentialElement>(GetDocument());
  element->setAttribute(html_names::kTypeAttr, AtomicString("federated"));
  element->setAttribute(html_names::kConfigurlAttr,
                        AtomicString("https://idp.com/config.json"));

  ConsoleMessageStorage& storage = GetPage().GetConsoleMessageStorage();
  wtf_size_t initial_size = storage.size();

  // Set invalid JSON in params.
  element->setAttribute(html_names::kParamsAttr,
                        AtomicString("{invalid: json}"));

  // Check that a console message was emitted.
  EXPECT_EQ(storage.size(), initial_size + 1);
  EXPECT_TRUE(
      storage.at(storage.size() - 1)->Message().contains("invalid JSON"));

  // Call GetFederatedRequestOptions and ensure it returns options but with
  // params_json unset (null).
  auto options = element->GetFederatedRequestOptions();
  ASSERT_TRUE(options);
  EXPECT_TRUE(options->params_json.IsNull());
  EXPECT_EQ(storage.size(), initial_size + 1);

  // Set valid JSON.
  element->setAttribute(html_names::kParamsAttr,
                        AtomicString("{\"valid\": \"json\"}"));
  options = element->GetFederatedRequestOptions();
  ASSERT_TRUE(options);
  EXPECT_EQ(options->params_json, "{\"valid\": \"json\"}");
  // No new error message should be added.
  EXPECT_EQ(storage.size(), initial_size + 1);
}

TEST_F(HTMLCredentialElementTest, ConfigUrlValidation) {
  NavigateTo(KURL("https://example.com"));
  auto* element = MakeGarbageCollected<HTMLCredentialElement>(GetDocument());
  element->setAttribute(html_names::kTypeAttr, AtomicString("federated"));

  ConsoleMessageStorage& storage = GetPage().GetConsoleMessageStorage();
  wtf_size_t initial_size = storage.size();

  // Set invalid URL in configurl.
  element->setAttribute(html_names::kConfigurlAttr, AtomicString("https://["));

  // Check that a console message was emitted.
  EXPECT_EQ(storage.size(), initial_size + 1);
  EXPECT_TRUE(
      storage.at(storage.size() - 1)->Message().contains("invalid URL"));

  // Should return nullptr because of invalid URL.
  EXPECT_FALSE(element->GetFederatedRequestOptions());
}

}  // namespace blink
