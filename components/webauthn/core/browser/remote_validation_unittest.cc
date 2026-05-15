// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/core/browser/remote_validation.h"

#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace webauthn {
namespace {

class RemoteValidationTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_ =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory_);
};

TEST_F(RemoteValidationTest, CspDisallowedInitial) {
  base::HistogramTester histograms;
  base::MockRepeatingClosure log_use_counter;
  EXPECT_CALL(log_use_counter, Run()).Times(1);

  std::vector<network::mojom::ContentSecurityPolicyPtr> policies;
  auto policy = network::mojom::ContentSecurityPolicy::New();
  policy->header = network::mojom::ContentSecurityPolicyHeader::New();
  policy->header->type = network::mojom::ContentSecurityPolicyType::kEnforce;
  policy->self_origin = network::mojom::CSPSource::New("https", "example.com",
                                                       443, "", false, false);
  auto source_list = network::mojom::CSPSourceList::New();
  source_list->sources.push_back(network::mojom::CSPSource::New(
      "https", "allowed.com", 443, "", false, false));
  policy->directives[network::mojom::CSPDirectiveName::ConnectSrc] =
      std::move(source_list);
  policies.push_back(std::move(policy));

  base::RunLoop run_loop;
  auto validation = RemoteValidation::Create(
      url::Origin::Create(GURL("https://example.com")), "disallowed.com",
      shared_url_loader_factory_, std::move(policies), log_use_counter.Get(),
      base::BindLambdaForTesting(
          [&](ValidationStatus status) { run_loop.Quit(); }));

  test_url_loader_factory_.AddResponse(
      "https://disallowed.com/.well-known/webauthn", "");
  run_loop.Run();

  histograms.ExpectUniqueSample("WebAuthentication.CspAllow.Remote", false, 1);
}

TEST_F(RemoteValidationTest, CspAllowedInitial) {
  base::HistogramTester histograms;
  base::MockRepeatingClosure log_use_counter;
  EXPECT_CALL(log_use_counter, Run()).Times(0);

  std::vector<network::mojom::ContentSecurityPolicyPtr> policies;
  auto policy = network::mojom::ContentSecurityPolicy::New();
  policy->header = network::mojom::ContentSecurityPolicyHeader::New();
  policy->header->type = network::mojom::ContentSecurityPolicyType::kEnforce;
  policy->self_origin = network::mojom::CSPSource::New("https", "example.com",
                                                       443, "", false, false);
  auto source_list = network::mojom::CSPSourceList::New();
  source_list->sources.push_back(network::mojom::CSPSource::New(
      "https", "allowed.com", 443, "", false, false));
  policy->directives[network::mojom::CSPDirectiveName::ConnectSrc] =
      std::move(source_list);
  policies.push_back(std::move(policy));

  base::RunLoop run_loop;
  auto validation = RemoteValidation::Create(
      url::Origin::Create(GURL("https://example.com")), "allowed.com",
      shared_url_loader_factory_, std::move(policies), log_use_counter.Get(),
      base::BindLambdaForTesting(
          [&](ValidationStatus status) { run_loop.Quit(); }));

  test_url_loader_factory_.AddResponse(
      "https://allowed.com/.well-known/webauthn", "");
  run_loop.Run();

  histograms.ExpectUniqueSample("WebAuthentication.CspAllow.Remote", true, 1);
}

TEST_F(RemoteValidationTest, CspDisallowedRedirect) {
  base::HistogramTester histograms;
  base::MockRepeatingClosure log_use_counter;
  // Called once for the redirect.
  EXPECT_CALL(log_use_counter, Run()).Times(1);

  std::vector<network::mojom::ContentSecurityPolicyPtr> policies;
  auto policy = network::mojom::ContentSecurityPolicy::New();
  policy->header = network::mojom::ContentSecurityPolicyHeader::New();
  policy->header->type = network::mojom::ContentSecurityPolicyType::kEnforce;
  policy->self_origin = network::mojom::CSPSource::New("https", "example.com",
                                                       443, "", false, false);
  auto source_list = network::mojom::CSPSourceList::New();
  source_list->sources.push_back(network::mojom::CSPSource::New(
      "https", "allowed.com", 443, "", false, false));
  policy->directives[network::mojom::CSPDirectiveName::ConnectSrc] =
      std::move(source_list);
  policies.push_back(std::move(policy));

  base::RunLoop run_loop;
  auto validation = RemoteValidation::Create(
      url::Origin::Create(GURL("https://example.com")), "allowed.com",
      shared_url_loader_factory_, std::move(policies), log_use_counter.Get(),
      base::BindLambdaForTesting(
          [&](ValidationStatus status) { run_loop.Quit(); }));

  net::RedirectInfo redirect_info;
  redirect_info.new_url = GURL("https://disallowed.com/somewhere");
  redirect_info.status_code = 302;

  network::TestURLLoaderFactory::Redirects redirects;
  redirects.emplace_back(redirect_info, network::mojom::URLResponseHead::New());

  test_url_loader_factory_.AddResponse(
      GURL("https://allowed.com/.well-known/webauthn"),
      network::mojom::URLResponseHead::New(), "",
      network::URLLoaderCompletionStatus(net::OK), std::move(redirects));
  run_loop.Run();

  histograms.ExpectUniqueSample("WebAuthentication.CspAllow.Remote", false, 1);
}

}  // namespace
}  // namespace webauthn
