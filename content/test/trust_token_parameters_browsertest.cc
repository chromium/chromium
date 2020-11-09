// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/common/network_service_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/url_loader_monitor.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "net/base/escape.h"
#include "net/dns/mock_host_resolver.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "services/network/public/mojom/url_loader.mojom-shared.h"
#include "services/network/trust_tokens/test/trust_token_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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
        network::features::kTrustTokens,
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

  EXPECT_TRUE(
      ExecJs(shell(), JsReplace("fetch($1, {trustToken: ", trust_token_url) +
                          expected_params_and_serialization.serialized_params +
                          "});"));

  monitor.WaitForUrls();
  base::Optional<network::ResourceRequest> request =
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

  GURL url(embedded_test_server()->GetURL("/title1.html"));
  GURL trust_token_url(embedded_test_server()->GetURL("/title2.html"));

  URLLoaderMonitor monitor({trust_token_url});

  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_TRUE(ExecJs(
      shell(), JsReplace("let iframe = document.createElement('iframe');"
                         "iframe.src = $1;"
                         "iframe.trustToken = $2;"
                         "document.body.appendChild(iframe);",
                         trust_token_url,
                         expected_params_and_serialization.serialized_params)));

  monitor.WaitForUrls();
  base::Optional<network::ResourceRequest> request =
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
             base::StringPrintf(
                 JsReplace("let request = new XMLHttpRequest();"
                           "request.open($1, $2);"
                           "request.setTrustToken(%s);"
                           "request.send();",
                           "GET", trust_token_url)
                     .c_str(),
                 expected_params_and_serialization.serialized_params.c_str())));

  monitor.WaitForUrls();
  base::Optional<network::ResourceRequest> request =
      monitor.GetRequestInfo(trust_token_url);
  ASSERT_TRUE(request);

  EXPECT_TRUE(request->trust_token_params.as_ptr().Equals(
      expected_params_and_serialization.params));
}

class TrustTokenFeaturePolicyBrowsertest : public ContentBrowserTest {
 public:
  TrustTokenFeaturePolicyBrowsertest() {
    features_.InitAndEnableFeature(network::features::kTrustTokens);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
  }

 protected:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(TrustTokenFeaturePolicyBrowsertest,
                       PassesNegativeValueToFactoryParams) {
  // Since the trust-token-redemption Feature Policy feature is disabled by
  // default in cross-site frames, the child's URLLoaderFactoryParams should be
  // populated with TrustTokenRedemptionPolicy::kForbid.

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

              ASSERT_THAT(params->trust_token_redemption_policy,
                          network::mojom::TrustTokenRedemptionPolicy::kForbid);
              run_loop.Quit();
            }
          }));

  EXPECT_TRUE(NavigateToURL(shell(), url));

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(TrustTokenFeaturePolicyBrowsertest,
                       PassesPositiveValueToFactoryParams) {
  // Even though the trust-token-redemption Feature Policy feature is disabled
  // by default in cross-site frames, the allow attribute on the iframe enables
  // it for the b.com frame, so the child's URLLoaderFactoryParams should be
  // populated with TrustTokenRedemptionPolicy::kPotentiallyPermit.

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(
      "a.com",
      "/cross_site_iframe_factory.html?a(b{allow-trust-token-redemption})"));

  base::RunLoop run_loop;
  ShellContentBrowserClient::Get()->set_url_loader_factory_params_callback(
      base::BindLambdaForTesting(
          [&](const network::mojom::URLLoaderFactoryParams* params,
              const url::Origin& origin, bool unused_is_for_isolated_world) {
            if (base::Contains(origin.host(), "b")) {
              ASSERT_TRUE(params);

              ASSERT_THAT(params->trust_token_redemption_policy,
                          network::mojom::TrustTokenRedemptionPolicy::
                              kPotentiallyPermit);

              run_loop.Quit();
            }
          }));

  EXPECT_TRUE(NavigateToURL(shell(), url));

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(TrustTokenFeaturePolicyBrowsertest,
                       PassesNegativeValueToFactoryParamsAfterCrash) {
  // Since the trust-token-redemption Feature Policy feature is disabled by
  // default in cross-site frames, the child's URLLoaderFactoryParams should be
  // populated with TrustTokenRedemptionPolicy::kForbid.
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

              ASSERT_THAT(params->trust_token_redemption_policy,
                          network::mojom::TrustTokenRedemptionPolicy::kForbid);
              run_loop.Quit();
            }
          }));

  SimulateNetworkServiceCrash();
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(TrustTokenFeaturePolicyBrowsertest,
                       PassesPositiveValueToFactoryParamsAfterCrash) {
  // Even though the trust-token-redemption Feature Policy feature is disabled
  // by default in cross-site frames, the allow attribute on the iframe enables
  // it for the b.com frame, so the child's URLLoaderFactoryParams should be
  // populated with TrustTokenRedemptionPolicy::kPotentiallyPermit.
  //
  // In particular, this should be true for factory params repopulated after a
  // network service crash!

  // Can't test this on bots that use an in-process network service.
  if (IsInProcessNetworkService())
    return;

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(
      "a.com",
      "/cross_site_iframe_factory.html?a(b{allow-trust-token-redemption})"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  base::RunLoop run_loop;
  ShellContentBrowserClient::Get()->set_url_loader_factory_params_callback(
      base::BindLambdaForTesting(
          [&](const network::mojom::URLLoaderFactoryParams* params,
              const url::Origin& origin, bool unused_is_for_isolated_world) {
            if (base::Contains(origin.host(), "b")) {
              ASSERT_TRUE(params);

              ASSERT_THAT(params->trust_token_redemption_policy,
                          network::mojom::TrustTokenRedemptionPolicy::
                              kPotentiallyPermit);

              run_loop.Quit();
            }
          }));

  SimulateNetworkServiceCrash();
  run_loop.Run();
}

}  // namespace content
