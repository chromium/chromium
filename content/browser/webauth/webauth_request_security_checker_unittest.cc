// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/webauth_request_security_checker.h"

#include <string_view>

#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_web_contents_factory.h"
#include "device/fido/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_util.h"

namespace content {
namespace {

blink::ParsedPermissionsPolicy CreatePolicyToAllowWebAuthn() {
  return {blink::ParsedPermissionsPolicyDeclaration(
      blink::mojom::PermissionsPolicyFeature::kPublicKeyCredentialsGet,
      /*allowed_origins=*/{}, /*self_if_matches=*/std::nullopt,
      /*matches_all_origins=*/true,
      /*matches_opaque_src=*/false)};
}

// The default policy allows same-origin with ancestors, but this creates one
// with value 'none'.
blink::ParsedPermissionsPolicy CreatePolicyToDenyWebAuthn() {
  return {blink::ParsedPermissionsPolicyDeclaration(
      blink::mojom::PermissionsPolicyFeature::kPublicKeyCredentialsGet,
      /*allowed_origins=*/{}, /*self_if_matches=*/std::nullopt,
      /*matches_all_origins=*/false,
      /*matches_opaque_src=*/false)};
}

blink::ParsedPermissionsPolicy CreatePolicyToAllowWebPayments() {
  return {blink::ParsedPermissionsPolicyDeclaration(
      blink::mojom::PermissionsPolicyFeature::kPayment, /*allowed_origins=*/{},
      /*self_if_matches=*/std::nullopt,
      /*matches_all_origins=*/true, /*matches_opaque_src=*/false)};
}

struct TestCase {
  TestCase(const std::string_view& url,
           const blink::ParsedPermissionsPolicy& policy,
           WebAuthRequestSecurityChecker::RequestType request_type,
           bool expected_is_cross_origin,
           blink::mojom::AuthenticatorStatus expected_status)
      : url(url),
        policy(policy),
        request_type(request_type),
        expected_is_cross_origin(expected_is_cross_origin),
        expected_status(expected_status) {}

  ~TestCase() = default;

  const std::string_view url;
  const blink::ParsedPermissionsPolicy policy;
  const WebAuthRequestSecurityChecker::RequestType request_type;
  const bool expected_is_cross_origin;
  const blink::mojom::AuthenticatorStatus expected_status;
};

std::ostream& operator<<(std::ostream& out, const TestCase& test_case) {
  out << test_case.url << " ";
  switch (test_case.request_type) {
    case WebAuthRequestSecurityChecker::RequestType::kGetAssertion:
      out << "Get Assertion";
      break;
    case WebAuthRequestSecurityChecker::RequestType::
        kGetPaymentCredentialAssertion:
      out << "Get Payment Credential Assertion";
      break;
    case WebAuthRequestSecurityChecker::RequestType::kMakePaymentCredential:
      out << "Make Payment Credential";
      break;
    case WebAuthRequestSecurityChecker::RequestType::kMakeCredential:
      out << "Make Credential";
      break;
    case WebAuthRequestSecurityChecker::RequestType::kReport:
      out << "Report";
      break;
  }
  return out;
}

class WebAuthRequestSecurityCheckerTest
    : public testing::TestWithParam<TestCase> {
 protected:
  WebAuthRequestSecurityCheckerTest()
      : web_contents_(web_contents_factory_.CreateWebContents(&context_)) {}

  ~WebAuthRequestSecurityCheckerTest() override = default;

  content::WebContents* web_contents() const { return web_contents_; }

 private:
  // Must be first because ScopedFeatureList must be initialized before other
  // threads are started.
  base::test::ScopedFeatureList features_{
      /*enable_feature=*/features::kSecurePaymentConfirmation};
  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext context_;
  content::TestWebContentsFactory web_contents_factory_;
  raw_ptr<content::WebContents>
      web_contents_;  // Owned by `web_contents_factory_`.
};

TEST_P(WebAuthRequestSecurityCheckerTest, ValidateAncestorOrigins) {
  RenderFrameHost* main_frame =
      NavigationSimulator::NavigateAndCommitFromBrowser(
          web_contents(), GURL("https://same-origin.com"));
  ASSERT_NE(nullptr, main_frame);
  RenderFrameHostTester* tester = RenderFrameHostTester::For(main_frame);
  RenderFrameHost* sub_frame =
      tester->AppendChildWithPolicy("sub_frame", GetParam().policy);
  ASSERT_NE(nullptr, sub_frame);
  sub_frame = NavigationSimulator::NavigateAndCommitFromDocument(
      GURL(GetParam().url), sub_frame);
  scoped_refptr<WebAuthRequestSecurityChecker> checker =
      static_cast<RenderFrameHostImpl*>(sub_frame)
          ->GetWebAuthRequestSecurityChecker();

  bool actual_is_cross_origin = false;
  blink::mojom::AuthenticatorStatus actual_status =
      checker->ValidateAncestorOrigins(
          url::Origin::Create(GURL(GetParam().url)), GetParam().request_type,
          &actual_is_cross_origin);

  EXPECT_EQ(GetParam().expected_status, actual_status);
  EXPECT_EQ(GetParam().expected_is_cross_origin, actual_is_cross_origin);
}

INSTANTIATE_TEST_SUITE_P(
    ProhibitCrossOrigin,
    WebAuthRequestSecurityCheckerTest,
    testing::Values(
        TestCase("https://same-origin.com",
                 blink::ParsedPermissionsPolicy(),
                 WebAuthRequestSecurityChecker::RequestType::kGetAssertion,
                 /*expected_is_cross_origin=*/false,
                 blink::mojom::AuthenticatorStatus::SUCCESS),
        TestCase("https://cross-origin.com",
                 blink::ParsedPermissionsPolicy(),
                 WebAuthRequestSecurityChecker::RequestType::kGetAssertion,
                 /*expected_is_cross_origin=*/true,
                 blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR),
        TestCase("https://same-origin.com",
                 blink::ParsedPermissionsPolicy(),
                 WebAuthRequestSecurityChecker::RequestType::kMakeCredential,
                 /*expected_is_cross_origin=*/false,
                 blink::mojom::AuthenticatorStatus::SUCCESS),
        TestCase("https://cross-origin.com",
                 blink::ParsedPermissionsPolicy(),
                 WebAuthRequestSecurityChecker::RequestType::kMakeCredential,
                 /*expected_is_cross_origin=*/true,
                 blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR),
        TestCase(
            "https://same-origin.com",
            blink::ParsedPermissionsPolicy(),
            WebAuthRequestSecurityChecker::RequestType::kMakePaymentCredential,
            /*expected_is_cross_origin=*/false,
            blink::mojom::AuthenticatorStatus::SUCCESS),
        TestCase(
            "https://cross-origin.com",
            blink::ParsedPermissionsPolicy(),
            WebAuthRequestSecurityChecker::RequestType::kMakePaymentCredential,
            /*expected_is_cross_origin=*/true,
            blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR)));

INSTANTIATE_TEST_SUITE_P(
    AllowCrossOriginWebAuthn,
    WebAuthRequestSecurityCheckerTest,
    testing::Values(
        TestCase("https://same-origin.com",
                 CreatePolicyToAllowWebAuthn(),
                 WebAuthRequestSecurityChecker::RequestType::kGetAssertion,
                 /*expected_is_cross_origin=*/false,
                 blink::mojom::AuthenticatorStatus::SUCCESS),
        TestCase("https://cross-origin.com",
                 CreatePolicyToAllowWebAuthn(),
                 WebAuthRequestSecurityChecker::RequestType::kGetAssertion,
                 /*expected_is_cross_origin=*/true,
                 blink::mojom::AuthenticatorStatus::SUCCESS),
        TestCase("https://same-origin.com",
                 CreatePolicyToAllowWebAuthn(),
                 WebAuthRequestSecurityChecker::RequestType::kMakeCredential,
                 /*expected_is_cross_origin=*/false,
                 blink::mojom::AuthenticatorStatus::SUCCESS),
        TestCase("https://cross-origin.com",
                 CreatePolicyToAllowWebAuthn(),
                 WebAuthRequestSecurityChecker::RequestType::kMakeCredential,
                 /*expected_is_cross_origin=*/true,
                 blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR),
        TestCase(
            "https://same-origin.com",
            CreatePolicyToAllowWebAuthn(),
            WebAuthRequestSecurityChecker::RequestType::kMakePaymentCredential,
            /*expected_is_cross_origin=*/false,
            blink::mojom::AuthenticatorStatus::SUCCESS),
        TestCase(
            "https://cross-origin.com",
            CreatePolicyToAllowWebAuthn(),
            WebAuthRequestSecurityChecker::RequestType::kMakePaymentCredential,
            /*expected_is_cross_origin=*/true,
            blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR)));

INSTANTIATE_TEST_SUITE_P(
    AllowCrossOriginPay,
    WebAuthRequestSecurityCheckerTest,
    testing::Values(
        TestCase("https://same-origin.com",
                 CreatePolicyToAllowWebPayments(),
                 WebAuthRequestSecurityChecker::RequestType::kGetAssertion,
                 /*expected_is_cross_origin=*/false,
                 blink::mojom::AuthenticatorStatus::SUCCESS),
        TestCase("https://cross-origin.com",
                 CreatePolicyToAllowWebPayments(),
                 WebAuthRequestSecurityChecker::RequestType::kGetAssertion,
                 /*expected_is_cross_origin=*/true,
                 blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR),
        TestCase("https://same-origin.com",
                 CreatePolicyToAllowWebPayments(),
                 WebAuthRequestSecurityChecker::RequestType::kMakeCredential,
                 /*expected_is_cross_origin=*/false,
                 blink::mojom::AuthenticatorStatus::SUCCESS),
        TestCase("https://cross-origin.com",
                 CreatePolicyToAllowWebPayments(),
                 WebAuthRequestSecurityChecker::RequestType::kMakeCredential,
                 /*expected_is_cross_origin=*/true,
                 blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR),
        TestCase(
            "https://same-origin.com",
            CreatePolicyToAllowWebPayments(),
            WebAuthRequestSecurityChecker::RequestType::kMakePaymentCredential,
            /*expected_is_cross_origin=*/false,
            blink::mojom::AuthenticatorStatus::SUCCESS),
        TestCase(
            "https://cross-origin.com",
            CreatePolicyToAllowWebPayments(),
            WebAuthRequestSecurityChecker::RequestType::kMakePaymentCredential,
            /*expected_is_cross_origin=*/true,
            blink::mojom::AuthenticatorStatus::SUCCESS)));

struct SingleFrameTestCase {
  SingleFrameTestCase(const blink::ParsedPermissionsPolicy& policy,
                      WebAuthRequestSecurityChecker::RequestType request_type,
                      blink::mojom::AuthenticatorStatus expected_status)
      : policy(policy),
        request_type(request_type),
        expected_status(expected_status) {}

  ~SingleFrameTestCase() = default;

  const blink::ParsedPermissionsPolicy policy;
  const WebAuthRequestSecurityChecker::RequestType request_type;
  const blink::mojom::AuthenticatorStatus expected_status;
};

class WebAuthRequestSecurityCheckerSingleFrameTest
    : public testing::TestWithParam<SingleFrameTestCase> {
 protected:
  WebAuthRequestSecurityCheckerSingleFrameTest()
      : web_contents_(web_contents_factory_.CreateWebContents(&context_)) {}

  ~WebAuthRequestSecurityCheckerSingleFrameTest() override = default;

  content::WebContents* web_contents() const { return web_contents_; }

 private:
  // Must be first because ScopedFeatureList must be initialized before other
  // threads are started.
  base::test::ScopedFeatureList features_{
      /*enable_feature=*/features::kSecurePaymentConfirmation};
  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext context_;
  content::TestWebContentsFactory web_contents_factory_;
  raw_ptr<content::WebContents>
      web_contents_;  // Owned by `web_contents_factory_`.
};

TEST_P(WebAuthRequestSecurityCheckerSingleFrameTest,
       ValidateAncestorOriginsOnRoot) {
  auto navigation = NavigationSimulator::CreateBrowserInitiated(
      GURL("https://same-origin.com"), web_contents());
  navigation->SetPermissionsPolicyHeader(GetParam().policy);
  navigation->Commit();
  ASSERT_NE(nullptr, web_contents()->GetPrimaryMainFrame());

  scoped_refptr<WebAuthRequestSecurityChecker> checker =
      static_cast<RenderFrameHostImpl*>(web_contents()->GetPrimaryMainFrame())
          ->GetWebAuthRequestSecurityChecker();

  bool actual_is_cross_origin = false;
  blink::mojom::AuthenticatorStatus actual_status =
      checker->ValidateAncestorOrigins(
          url::Origin::Create(GURL("https://same-origin.com")),
          GetParam().request_type, &actual_is_cross_origin);

  EXPECT_EQ(GetParam().expected_status, actual_status);
  EXPECT_EQ(false, actual_is_cross_origin);
}

INSTANTIATE_TEST_SUITE_P(
    WebAuthnSingleFrame,
    WebAuthRequestSecurityCheckerSingleFrameTest,
    testing::Values(
        SingleFrameTestCase(
            CreatePolicyToAllowWebAuthn(),
            WebAuthRequestSecurityChecker::RequestType::kGetAssertion,
            blink::mojom::AuthenticatorStatus::SUCCESS),
        SingleFrameTestCase(
            CreatePolicyToDenyWebAuthn(),
            WebAuthRequestSecurityChecker::RequestType::kGetAssertion,
            blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR),
        SingleFrameTestCase(
            CreatePolicyToAllowWebAuthn(),
            WebAuthRequestSecurityChecker::RequestType::kMakeCredential,
            blink::mojom::AuthenticatorStatus::SUCCESS),
        SingleFrameTestCase(
            CreatePolicyToDenyWebAuthn(),
            WebAuthRequestSecurityChecker::RequestType::kMakeCredential,
            blink::mojom::AuthenticatorStatus::SUCCESS)));

class WebAuthRequestSecurityCheckerWellKnownJSONTest : public testing::Test {
 protected:
  blink::mojom::AuthenticatorStatus Test(std::string_view caller_origin_str,
                                         std::string_view json) {
    std::optional<base::Value> parsed =
        base::JSONReader::Read(json, base::JSON_PARSE_RFC);
    CHECK(parsed) << json;

    GURL caller_origin_url(caller_origin_str);
    CHECK(caller_origin_url.is_valid()) << caller_origin_str;

    return WebAuthRequestSecurityChecker::RemoteValidation::
        ValidateWellKnownJSON(url::Origin::Create(caller_origin_url), *parsed);
  }
};

TEST_F(WebAuthRequestSecurityCheckerWellKnownJSONTest, Inputs) {
  const base::test::ScopedFeatureList scoped_feature_list{
      device::kWebAuthnRelatedOrigin};

  struct TestCase {
    const char* json;
    blink::mojom::AuthenticatorStatus expected;
  };
  constexpr blink::mojom::AuthenticatorStatus parse_error =
      blink::mojom::AuthenticatorStatus::BAD_RELYING_PARTY_ID_JSON_PARSE_ERROR;
  constexpr blink::mojom::AuthenticatorStatus ok =
      blink::mojom::AuthenticatorStatus::SUCCESS;
  constexpr blink::mojom::AuthenticatorStatus no_match =
      blink::mojom::AuthenticatorStatus::BAD_RELYING_PARTY_ID_NO_JSON_MATCH;
  constexpr blink::mojom::AuthenticatorStatus no_match_hit_limits = blink::
      mojom::AuthenticatorStatus::BAD_RELYING_PARTY_ID_NO_JSON_MATCH_HIT_LIMITS;

  static const TestCase kTestCases[] = {
      {R"([])", parse_error},
      {R"({})", parse_error},
      {R"({"foo": "bar"})", parse_error},
      {R"({"origins": "bar"})", parse_error},
      {R"({"origins": []})", no_match},
      {R"({"origins": [1]})", parse_error},
      {R"({"origins": ["https://foo.com"]})", ok},
      {R"({"origins": ["https://foo2.com"]})", no_match},
      {R"({"origins": ["https://com"]})", no_match},
      {R"({"origins": ["other://foo.com"]})", no_match},
      {R"({"origins": [
            "https://a.com",
            "https://b.com",
            "https://c.com",
            "https://d.com",
            "https://foo.com"
          ]})",
       ok},
      // Too many eTLD+1 labels.
      {R"({"origins": [
            "https://a.com",
            "https://b.com",
            "https://c.com",
            "https://d.com",
            "https://e.com",
            "https://foo.com"
          ]})",
       no_match_hit_limits},
      // Too many eTLD+1 labels, but foo.com isn't at the end so will be
      // processed.
      {R"({"origins": [
            "https://a.com",
            "https://b.com",
            "https://c.com",
            "https://d.com",
            "https://foo.com",
            "https://e.com"
          ]})",
       ok},
      {R"({"origins": [
            "https://foo.co.uk",
            "https://foo.de",
            "https://foo.in",
            "https://foo.net",
            "https://foo.org",
            "https://foo.com"
          ]})",
       ok},
  };

  for (const auto& test : kTestCases) {
    SCOPED_TRACE(test.json);

    EXPECT_EQ(test.expected, Test("https://foo.com", test.json));
  }
}

}  // namespace
}  // namespace content
