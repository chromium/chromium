// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/contains.h"
#include "base/strings/escape.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/url_loader_monitor.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "net/dns/mock_host_resolver.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "services/network/public/mojom/url_loader.mojom-shared.h"
#include "services/network/test/trust_token_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

// These integration tests verify that calling the Fetch API with Trust Tokens
// parameters results in the parameters' counterparts appearing downstream in
// network::ResourceRequest.
//
// Separately, Blink layout tests check that the API correctly rejects invalid
// input.

namespace content {

class TrustTokenParametersBrowsertest
    : public ::testing::WithParamInterface<network::TrustTokenTestParameters>,
      public ContentBrowserTest {
 public:
  TrustTokenParametersBrowsertest() {
    auto& field_trial_param =
        network::features::kTrustTokenOperationsRequiringOriginTrial;
    features_.InitAndEnableFeatureWithParameters(
        network::features::kFledgePst,
        {{field_trial_param.name,
          field_trial_param.GetName(
              network::features::TrustTokenOriginTrialSpec::
                  kOriginTrialNotRequired)}});
  }

 protected:
  base::test::ScopedFeatureList features_;
};

INSTANTIATE_TEST_SUITE_P(
    WithIssuanceParameters,
    TrustTokenParametersBrowsertest,
    testing::ValuesIn(network::kIssuanceTrustTokenTestParameters));

INSTANTIATE_TEST_SUITE_P(
    WithRedemptionParameters,
    TrustTokenParametersBrowsertest,
    testing::ValuesIn(network::kRedemptionTrustTokenTestParameters));

INSTANTIATE_TEST_SUITE_P(
    WithSigningParameters,
    TrustTokenParametersBrowsertest,
    testing::ValuesIn(network::kSigningTrustTokenTestParameters));

IN_PROC_BROWSER_TEST_P(TrustTokenParametersBrowsertest,
                       PopulatesResourceRequestViaFetch) {
  ASSERT_TRUE(embedded_test_server()->Start());

  network::TrustTokenParametersAndSerialization
      expected_params_and_serialization =
          network::SerializeTrustTokenParametersAndConstructExpectation(
              GetParam());

  GURL url(embedded_test_server()->GetURL("/title1.html"));
  GURL trust_token_url(embedded_test_server()->GetURL("/title2.html"));

  URLLoaderMonitor monitor({trust_token_url});

  EXPECT_TRUE(NavigateToURL(shell(), url));

  ExecuteScriptAsync(
      shell(), JsReplace("fetch($1, {privateToken: ", trust_token_url) +
                   expected_params_and_serialization.serialized_params + "});");

  monitor.WaitForUrls();
  std::optional<network::ResourceRequest> request =
      monitor.GetRequestInfo(trust_token_url);
  ASSERT_TRUE(request);
  ASSERT_TRUE(request->trust_token_params);
  EXPECT_TRUE(request->trust_token_params.as_ptr().Equals(
      expected_params_and_serialization.params));
}

IN_PROC_BROWSER_TEST_P(TrustTokenParametersBrowsertest,
                       PopulatesResourceRequestViaIframe) {
  ASSERT_TRUE(embedded_test_server()->Start());

  network::TrustTokenParametersAndSerialization
      expected_params_and_serialization =
          network::SerializeTrustTokenParametersAndConstructExpectation(
              GetParam());

  // In the iframe interface to private state tokens, we only accept the
  // kSigning variant, i.e. the send-redemption-record operation.
  if (expected_params_and_serialization.params->operation !=
      network::mojom::TrustTokenOperationType::kSigning) {
    return;
  }

  GURL url(embedded_test_server()->GetURL("/title1.html"));
  GURL trust_token_url(embedded_test_server()->GetURL("/title2.html"));

  URLLoaderMonitor monitor({trust_token_url});

  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_TRUE(ExecJs(
      shell(), JsReplace("let iframe = document.createElement('iframe');"
                         "iframe.src = $1;"
                         "iframe.privateToken = $2;"
                         "document.body.appendChild(iframe);",
                         trust_token_url,
                         expected_params_and_serialization.serialized_params)));

  monitor.WaitForUrls();
  std::optional<network::ResourceRequest> request =
      monitor.GetRequestInfo(trust_token_url);
  ASSERT_TRUE(request);
  ASSERT_TRUE(request->trust_token_params);

  EXPECT_TRUE(request->trust_token_params.as_ptr().Equals(
      expected_params_and_serialization.params));
}

IN_PROC_BROWSER_TEST_P(TrustTokenParametersBrowsertest,
                       PopulatesResourceRequestViaXhr) {
  ASSERT_TRUE(embedded_test_server()->Start());

  network::TrustTokenParametersAndSerialization
      expected_params_and_serialization =
          network::SerializeTrustTokenParametersAndConstructExpectation(
              GetParam());

  GURL url(embedded_test_server()->GetURL("/title1.html"));
  GURL trust_token_url(embedded_test_server()->GetURL("/title2.html"));

  URLLoaderMonitor monitor({trust_token_url});

  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_TRUE(
      ExecJs(shell(),
             base::StringPrintfNonConstexpr(
                 JsReplace("let request = new XMLHttpRequest();"
                           "request.open($1, $2);"
                           "request.setPrivateToken(%s);"
                           "request.send();",
                           "GET", trust_token_url)
                     .c_str(),
                 expected_params_and_serialization.serialized_params.c_str())));

  monitor.WaitForUrls();
  std::optional<network::ResourceRequest> request =
      monitor.GetRequestInfo(trust_token_url);
  ASSERT_TRUE(request);

  EXPECT_TRUE(request->trust_token_params.as_ptr().Equals(
      expected_params_and_serialization.params));
}

class TrustTokenPermissionsPolicyBrowsertest : public ContentBrowserTest {
 public:
  TrustTokenPermissionsPolicyBrowsertest() {
    features_.InitAndEnableFeature(network::features::kPrivateStateTokens);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
  }

 protected:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(TrustTokenPermissionsPolicyBrowsertest,
                       PassesNegativeValueToFactoryParams) {
  // Since the private-state-token-redemption Permissions Policy feature is
  // disabled by default in cross-site frames, the child's
  // URLLoaderFactoryParams should be populated with
  // TrustTokenOpertationPolicyVerdict::kForbid.

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));

  base::RunLoop run_loop;
  ShellContentBrowserClient::Get()->set_url_loader_factory_params_callback(
      base::BindLambdaForTesting(
          [&](const network::mojom::URLLoaderFactoryParams* params,
              const url::Origin& origin, bool unused_is_for_isolated_world) {
            if (base::Contains(origin.host(), 'b')) {
              ASSERT_TRUE(params);

              ASSERT_THAT(
                  params->trust_token_redemption_policy,
                  network::mojom::TrustTokenOperationPolicyVerdict::kForbid);
              ASSERT_THAT(
                  params->trust_token_issuance_policy,
                  network::mojom::TrustTokenOperationPolicyVerdict::kForbid);
              run_loop.Quit();
            }
          }));

  EXPECT_TRUE(NavigateToURL(shell(), url));

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(TrustTokenPermissionsPolicyBrowsertest,
                       PassesPositiveValueToFactoryParams) {
  // Even though the private-state-token-redemption Permissions Policy feature
  // is disabled by default in cross-site frames, the allow attribute on the
  // iframe enables it for the b.com frame, so the child's
  // URLLoaderFactoryParams should be populated with
  // TrustTokenOperationPolicyVerdict::kPotentiallyPermit.

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(
      embedded_test_server()->GetURL("a.com",
                                     "/cross_site_iframe_factory.html?a(b{"
                                     "allow-private-state-token-redemption})"));

  base::RunLoop run_loop;
  ShellContentBrowserClient::Get()->set_url_loader_factory_params_callback(
      base::BindLambdaForTesting(
          [&](const network::mojom::URLLoaderFactoryParams* params,
              const url::Origin& origin, bool unused_is_for_isolated_world) {
            if (base::Contains(origin.host(), "b")) {
              ASSERT_TRUE(params);

              ASSERT_THAT(params->trust_token_redemption_policy,
                          network::mojom::TrustTokenOperationPolicyVerdict::
                              kPotentiallyPermit);
              ASSERT_THAT(
                  params->trust_token_issuance_policy,
                  network::mojom::TrustTokenOperationPolicyVerdict::kForbid);

              run_loop.Quit();
            }
          }));

  EXPECT_TRUE(NavigateToURL(shell(), url));

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(TrustTokenPermissionsPolicyBrowsertest,
                       PassesPositiveIssuanceValueToFactoryParams) {
  // Even though the private-state-token-issuance Permissions Policy feature is
  // disabled by default in cross-site frames, the allow attribute on the iframe
  // enables it for the b.com frame, so the child's URLLoaderFactoryParams
  // should be populated with
  // TrustTokenOperationPolicyVerdict::kPotentiallyPermit.

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(
      embedded_test_server()->GetURL("a.com",
                                     "/cross_site_iframe_factory.html?a(b{"
                                     "allow-private-state-token-issuance})"));

  base::RunLoop run_loop;
  ShellContentBrowserClient::Get()->set_url_loader_factory_params_callback(
      base::BindLambdaForTesting(
          [&](const network::mojom::URLLoaderFactoryParams* params,
              const url::Origin& origin, bool unused_is_for_isolated_world) {
            if (base::Contains(origin.host(), "b")) {
              ASSERT_TRUE(params);

              ASSERT_THAT(
                  params->trust_token_redemption_policy,
                  network::mojom::TrustTokenOperationPolicyVerdict::kForbid);
              ASSERT_THAT(params->trust_token_issuance_policy,
                          network::mojom::TrustTokenOperationPolicyVerdict::
                              kPotentiallyPermit);

              run_loop.Quit();
            }
          }));

  EXPECT_TRUE(NavigateToURL(shell(), url));

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(TrustTokenPermissionsPolicyBrowsertest,
                       PassesNegativeValueToFactoryParamsAfterCrash) {
  // Since the private-state-token-redemption Permissions Policy feature is
  // disabled by default in cross-site frames, the child's
  // URLLoaderFactoryParams should be populated with
  // TrustTokenOperationPolicyVerdict::kForbid.
  //
  // In particular, this should be true for factory params repopulated after a
  // network service crash!

  // Can't test this on bots that use an in-process network service.
  if (IsInProcessNetworkService())
    return;

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  base::RunLoop run_loop;
  ShellContentBrowserClient::Get()->set_url_loader_factory_params_callback(
      base::BindLambdaForTesting(
          [&](const network::mojom::URLLoaderFactoryParams* params,
              const url::Origin& origin, bool unused_is_for_isolated_world) {
            if (base::Contains(origin.host(), 'b')) {
              ASSERT_TRUE(params);

              ASSERT_THAT(
                  params->trust_token_redemption_policy,
                  network::mojom::TrustTokenOperationPolicyVerdict::kForbid);
              ASSERT_THAT(
                  params->trust_token_issuance_policy,
                  network::mojom::TrustTokenOperationPolicyVerdict::kForbid);
              run_loop.Quit();
            }
          }));

  SimulateNetworkServiceCrash();
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(TrustTokenPermissionsPolicyBrowsertest,
                       PassesPositiveValueToFactoryParamsAfterCrash) {
  // Even though the private-state-token-redemption Permissions Policy feature
  // is disabled by default in cross-site frames, the allow attribute on the
  // iframe enables it for the b.com frame, so the child's
  // URLLoaderFactoryParams should be populated with
  // TrustTokenOperationPolicyVerdict::kPotentiallyPermit.
  //
  // In particular, this should be true for factory params repopulated after a
  // network service crash!

  // Can't test this on bots that use an in-process network service.
  if (IsInProcessNetworkService())
    return;

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(
      embedded_test_server()->GetURL("a.com",
                                     "/cross_site_iframe_factory.html?a(b{"
                                     "allow-private-state-token-redemption})"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  base::RunLoop run_loop;
  ShellContentBrowserClient::Get()->set_url_loader_factory_params_callback(
      base::BindLambdaForTesting(
          [&](const network::mojom::URLLoaderFactoryParams* params,
              const url::Origin& origin, bool unused_is_for_isolated_world) {
            if (base::Contains(origin.host(), "b")) {
              ASSERT_TRUE(params);

              ASSERT_THAT(params->trust_token_redemption_policy,
                          network::mojom::TrustTokenOperationPolicyVerdict::
                              kPotentiallyPermit);
              ASSERT_THAT(
                  params->trust_token_issuance_policy,
                  network::mojom::TrustTokenOperationPolicyVerdict::kForbid);

              run_loop.Quit();
            }
          }));

  SimulateNetworkServiceCrash();
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(TrustTokenPermissionsPolicyBrowsertest,
                       PassesPositiveIssuanceValueToFactoryParamsAfterCrash) {
  // Even though the private-state-token-issuance Permissions Policy feature is
  // disabled by default in cross-site frames, the allow attribute on the iframe
  // enables it for the b.com frame, so the child's URLLoaderFactoryParams
  // should be populated with
  // TrustTokenOperationPolicyVerdict::kPotentiallyPermit.
  //
  // In particular, this should be true for factory params repopulated after a
  // network service crash!

  // Can't test this on bots that use an in-process network service.
  if (IsInProcessNetworkService()) {
    return;
  }

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(
      embedded_test_server()->GetURL("a.com",
                                     "/cross_site_iframe_factory.html?a(b{"
                                     "allow-private-state-token-issuance})"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  base::RunLoop run_loop;
  ShellContentBrowserClient::Get()->set_url_loader_factory_params_callback(
      base::BindLambdaForTesting(
          [&](const network::mojom::URLLoaderFactoryParams* params,
              const url::Origin& origin, bool unused_is_for_isolated_world) {
            if (base::Contains(origin.host(), "b")) {
              ASSERT_TRUE(params);

              ASSERT_THAT(
                  params->trust_token_redemption_policy,
                  network::mojom::TrustTokenOperationPolicyVerdict::kForbid);
              ASSERT_THAT(params->trust_token_issuance_policy,
                          network::mojom::TrustTokenOperationPolicyVerdict::
                              kPotentiallyPermit);

              run_loop.Quit();
            }
          }));

  SimulateNetworkServiceCrash();
  run_loop.Run();
}

constexpr char kPrivateStateTokenRedemptionPolicyHeader[] =
    "/set-header?Feature-Policy: private-state-token-redemption 'self'";

constexpr char kPrivateStateTokenIssuancePolicyHeader[] =
    "/set-header?Feature-Policy: private-state-token-issuance 'self'";

class TrustTokenPermissionsPolicyFencedFrameTest
    : public TrustTokenPermissionsPolicyBrowsertest,
      public ::testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  TrustTokenPermissionsPolicyFencedFrameTest()
      : policy_header_in_primary_page_(std::get<0>(GetParam())),
        policy_header_in_fenced_frame_page_(std::get<1>(GetParam())) {}

  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_helper_;
  }

 protected:
  const bool policy_header_in_primary_page_;
  const bool policy_header_in_fenced_frame_page_;

 private:
  content::test::FencedFrameTestHelper fenced_frame_helper_;
  base::test::ScopedFeatureList features_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         TrustTokenPermissionsPolicyFencedFrameTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool()));

IN_PROC_BROWSER_TEST_P(TrustTokenPermissionsPolicyFencedFrameTest,
                       PassesNegativeRedemptionValueToFactoryParams) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL primary_url(embedded_test_server()->GetURL(
      "a.com", policy_header_in_primary_page_
                   ? kPrivateStateTokenRedemptionPolicyHeader
                   : "/title1.html"));

  GURL fenced_frame_url(embedded_test_server()->GetURL(
      "b.com", policy_header_in_fenced_frame_page_
                   ? std::string(kPrivateStateTokenRedemptionPolicyHeader) +
                         "&Supports-Loading-Mode: fenced-frame"
                   : "/fenced_frames/title1.html"));

  EXPECT_TRUE(NavigateToURL(shell(), primary_url));

  base::RunLoop run_loop;
  ShellContentBrowserClient::Get()->set_url_loader_factory_params_callback(
      base::BindLambdaForTesting(
          [&](const network::mojom::URLLoaderFactoryParams* params,
              const url::Origin& origin, bool unused_is_for_isolated_world) {
            if (origin.host() != "b.com")
              return;
            EXPECT_TRUE(params);
            EXPECT_THAT(
                params->trust_token_redemption_policy,
                network::mojom::TrustTokenOperationPolicyVerdict::kForbid);
            run_loop.Quit();
          }));

  ASSERT_TRUE(fenced_frame_test_helper().CreateFencedFrame(
      shell()->web_contents()->GetPrimaryMainFrame(), fenced_frame_url));

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_P(TrustTokenPermissionsPolicyFencedFrameTest,
                       PassesNegativeIssuanceValueToFactoryParams) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL primary_url(embedded_test_server()->GetURL(
      "a.com", policy_header_in_primary_page_
                   ? kPrivateStateTokenIssuancePolicyHeader
                   : "/title1.html"));

  GURL fenced_frame_url(embedded_test_server()->GetURL(
      "b.com", policy_header_in_fenced_frame_page_
                   ? std::string(kPrivateStateTokenIssuancePolicyHeader) +
                         "&Supports-Loading-Mode: fenced-frame"
                   : "/fenced_frames/title1.html"));

  EXPECT_TRUE(NavigateToURL(shell(), primary_url));

  base::RunLoop run_loop;
  ShellContentBrowserClient::Get()->set_url_loader_factory_params_callback(
      base::BindLambdaForTesting(
          [&](const network::mojom::URLLoaderFactoryParams* params,
              const url::Origin& origin, bool unused_is_for_isolated_world) {
            if (origin.host() != "b.com") {
              return;
            }
            EXPECT_TRUE(params);
            EXPECT_THAT(
                params->trust_token_issuance_policy,
                network::mojom::TrustTokenOperationPolicyVerdict::kForbid);
            run_loop.Quit();
          }));

  ASSERT_TRUE(fenced_frame_test_helper().CreateFencedFrame(
      shell()->web_contents()->GetPrimaryMainFrame(), fenced_frame_url));

  run_loop.Run();
}

}  // namespace content
