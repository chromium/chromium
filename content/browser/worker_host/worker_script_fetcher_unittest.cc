// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/worker_host/worker_script_fetcher.h"

#include <vector>

#include "base/test/scoped_feature_list.h"
#include "content/common/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/worker/worker_main_script_load_params.mojom.h"
#include "url/gurl.h"

namespace content {

namespace {

blink::mojom::WorkerMainScriptLoadParamsPtr CreateParams(
    const std::vector<GURL>& url_list_via_service_worker,
    const std::vector<GURL>& redirect_infos) {
  blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params =
      blink::mojom::WorkerMainScriptLoadParams::New();

  main_script_load_params->response_head =
      network::mojom::URLResponseHead::New();

  if (!url_list_via_service_worker.empty()) {
    main_script_load_params->response_head->was_fetched_via_service_worker =
        true;
    main_script_load_params->response_head->url_list_via_service_worker =
        url_list_via_service_worker;
  }

  for (const GURL& url : redirect_infos) {
    net::RedirectInfo redirect_info;
    redirect_info.new_url = url;
    main_script_load_params->redirect_infos.push_back(redirect_info);
  }

  return main_script_load_params;
}

}  // namespace

TEST(WorkerScriptFetcherTest, DetermineFinalResponseUrl) {
  struct TestCase {
    GURL initial_request_url;
    std::vector<GURL> url_list_via_service_worker;
    std::vector<GURL> redirect_infos;
    GURL expected_final_response_url;
  };

  static const std::vector<TestCase> kTestCases = {
      {
          GURL("https://initial.com"),
          {},
          {},
          GURL("https://initial.com"),
      },
      {
          GURL("https://initial.com"),
          {GURL("https://url_list_1.com"), GURL("https://url_list_2.com")},
          {},
          GURL("https://url_list_2.com"),
      },
      {
          GURL("https://initial.com"),
          {},
          {GURL("https://redirect_1.com"), GURL("https://redirect_2.com")},
          GURL("https://redirect_2.com"),
      },
      {
          GURL("https://initial.com"),
          {GURL("https://url_list_1.com"), GURL("https://url_list_2.com")},
          {GURL("https://redirect_1.com"), GURL("https://redirect_2.com")},
          GURL("https://url_list_2.com"),
      },
      {
          GURL("blob:https://initial.com/uuid"),
          {GURL("https://url_list_1.com"), GURL("https://url_list_2.com")},
          {GURL("https://redirect_1.com"), GURL("https://redirect_2.com")},
          GURL("blob:https://initial.com/uuid"),
      },
  };

  for (const auto& test_case : kTestCases) {
    blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params =
        CreateParams(test_case.url_list_via_service_worker,
                     test_case.redirect_infos);

    GURL final_response_url = WorkerScriptFetcher::DetermineFinalResponseUrl(
        test_case.initial_request_url, main_script_load_params.get());

    EXPECT_EQ(final_response_url, test_case.expected_final_response_url);
  }
}

TEST(WorkerScriptFetcherTest, ComputePolicyContainerPolicies_NetworkResponse) {
  auto response_head = network::mojom::URLResponseHead::New();
  response_head->parsed_headers = network::mojom::ParsedHeaders::New();
  auto csp = network::mojom::ContentSecurityPolicy::New();
  csp->self_origin = network::mojom::CSPSource::New();
  csp->header = network::mojom::ContentSecurityPolicyHeader::New();
  csp->sandbox = network::mojom::WebSandboxFlags::kOrigin;
  response_head->parsed_headers->content_security_policy.push_back(
      std::move(csp));

  PolicyContainerPolicies policies =
      WorkerScriptFetcher::ComputePolicyContainerPoliciesForTesting(
          GURL("https://example.com/worker.js"), response_head.get(),
          /*creator_policies=*/nullptr);
  EXPECT_EQ(network::mojom::WebSandboxFlags::kOrigin,
            policies.sandbox_flags & network::mojom::WebSandboxFlags::kOrigin);
}

TEST(WorkerScriptFetcherTest, ComputePolicyContainerPolicies_LocalScheme) {
  auto response_head = network::mojom::URLResponseHead::New();
  response_head->parsed_headers = network::mojom::ParsedHeaders::New();

  PolicyContainerPolicies creator_policies;
  creator_policies.sandbox_flags = network::mojom::WebSandboxFlags::kOrigin;

  PolicyContainerPolicies policies =
      WorkerScriptFetcher::ComputePolicyContainerPoliciesForTesting(
          GURL("blob:https://example.com/uuid"), response_head.get(),
          &creator_policies);
  EXPECT_EQ(network::mojom::WebSandboxFlags::kOrigin,
            policies.sandbox_flags & network::mojom::WebSandboxFlags::kOrigin);
}

TEST(WorkerScriptFetcherTest, ComputePolicyContainerPolicies_FeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kServiceWorkerDropHandleForCSPSandboxedWorker);

  auto response_head = network::mojom::URLResponseHead::New();
  response_head->parsed_headers = network::mojom::ParsedHeaders::New();
  auto csp = network::mojom::ContentSecurityPolicy::New();
  csp->self_origin = network::mojom::CSPSource::New();
  csp->header = network::mojom::ContentSecurityPolicyHeader::New();
  csp->sandbox = network::mojom::WebSandboxFlags::kOrigin;
  response_head->parsed_headers->content_security_policy.push_back(
      std::move(csp));

  PolicyContainerPolicies policies =
      WorkerScriptFetcher::ComputePolicyContainerPoliciesForTesting(
          GURL("https://example.com/worker.js"), response_head.get(),
          /*creator_policies=*/nullptr);
  EXPECT_EQ(network::mojom::WebSandboxFlags::kNone, policies.sandbox_flags);
}

}  // namespace content
