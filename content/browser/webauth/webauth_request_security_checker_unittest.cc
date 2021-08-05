// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/webauth_request_security_checker.h"
#include "base/strings/string_piece.h"
#include "base/test/scoped_feature_list.h"
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

namespace content {
namespace {

blink::ParsedPermissionsPolicy CreatePolicyToAllowWebAuthn() {
  return {blink::ParsedPermissionsPolicyDeclaration(
      blink::mojom::PermissionsPolicyFeature::kPublicKeyCredentialsGet,
      /*values=*/{}, /*matches_all_origins=*/true,
      /*matches_opaque_src=*/false)};
}

blink::ParsedPermissionsPolicy CreatePolicyToAllowWebPayments() {
  return {blink::ParsedPermissionsPolicyDeclaration(
      blink::mojom::PermissionsPolicyFeature::kPayment, /*values=*/{},
      /*matches_all_origins=*/true, /*matches_opaque_src=*/false)};
}

struct TestCase {
  TestCase(const base::StringPiece& url,
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

  const base::StringPiece url;
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
    case WebAuthRequestSecurityChecker::RequestType::kMakePaymentCredential:
      out << "Make Payment Credential";
      break;
    case WebAuthRequestSecurityChecker::RequestType::kMakeCredential:
      out << "Make Credential";
      break;
  }
  return out;
}

class WebAuthRequestSecurityCheckerTest
    : public testing::TestWithParam<TestCase> {
 protected:
  WebAuthRequestSecurityCheckerTest()
      : web_contents_(web_contents_factory_.CreateWebContents(&context_)) {
    features_.InitWithFeatures(
        /*enabled_features=*/{features::kSecurePaymentConfirmation},
        /*disabled_features=*/{});
  }

  ~WebAuthRequestSecurityCheckerTest() override = default;

  content::WebContents* web_contents() const { return web_contents_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext context_;
  content::TestWebContentsFactory web_contents_factory_;
  content::WebContents* web_contents_;  // Owned by `web_contents_factory_`.
  base::test::ScopedFeatureList features_;
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

}  // namespace
}  // namespace content
