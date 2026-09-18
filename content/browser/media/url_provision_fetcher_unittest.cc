// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/url_provision_fetcher.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "content/public/browser/provision_fetcher_factory.h"
#include "media/base/media_switches.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/android_info.h"
#include "base/i18n/android_locale.h"
#include "base/strings/stringprintf.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace content {

namespace {

constexpr char kTestUrl[] = "https://test.com";
constexpr char kTestRequestBody[] = "request_body";
constexpr char kRequestParam[] = "signedRequest=request_body";

}  // namespace

class URLProvisionFetcherTest : public testing::Test {
 public:
  URLProvisionFetcherTest()
      : shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)),
        fetcher_(CreateProvisionFetcher(shared_url_loader_factory_)) {}

 protected:
  void CheckCommonRequestExpectations(
      const GURL& url,
      const network::TestURLLoaderFactory::PendingRequest** pending_request) {
    ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
    *pending_request = test_url_loader_factory_.GetPendingRequest(0);
    EXPECT_EQ((*pending_request)->request.method, "POST");
  }

  void SimulateResponse(const std::string& url) {
    test_url_loader_factory_.SimulateResponseForPendingRequest(url, "");
  }

  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  std::unique_ptr<media::ProvisionFetcher> fetcher_;
};

TEST_F(URLProvisionFetcherTest, SendsPostRequestWithBody) {
  const GURL expected_url(kTestUrl);

  base::RunLoop run_loop;
  fetcher_->Retrieve(expected_url, kTestRequestBody,
                     base::BindOnce([](bool, const std::string&) {
                     }).Then(run_loop.QuitClosure()));

  const network::TestURLLoaderFactory::PendingRequest* pending_request_ptr;
  CheckCommonRequestExpectations(expected_url, &pending_request_ptr);
  const auto& pending_request = pending_request_ptr->request;

  EXPECT_EQ(pending_request.url, expected_url);
  EXPECT_EQ(pending_request.headers.GetHeader("Content-Type"),
            "application/x-www-form-urlencoded");

  ASSERT_EQ(pending_request.request_body->elements()->size(), 1u);
  EXPECT_EQ(pending_request.request_body->elements()
                ->at(0)
                .As<network::DataElementBytes>()
                .AsStringView(),
            kRequestParam);

  SimulateResponse(kTestUrl);
  run_loop.Run();
}

TEST_F(URLProvisionFetcherTest, UserAgent) {

  std::string expected_user_agent;
#if BUILDFLAG(IS_ANDROID)
  expected_user_agent = base::StringPrintf(
      "Widevine CDM v1.0 (Linux; U; Android %d; %s; Build/%s; %s)",
      base::android::android_info::sdk_int(),
      std::string(base::i18n::GetAndroidDefaultLocale().tag_string()).c_str(),
      base::android::android_info::android_build_id(),
      base::android::android_info::build_type());
#else
  expected_user_agent = "Widevine CDM v1.0";
#endif

  auto fetcher = CreateProvisionFetcherWithUserAgent(shared_url_loader_factory_,
                                                     expected_user_agent);

  fetcher->Retrieve(GURL(kTestUrl), kTestRequestBody,
                    base::BindOnce([](bool, const std::string&) {}));

  EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);
  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
  EXPECT_EQ(expected_user_agent, pending_request->request.headers.GetHeader(
                                     net::HttpRequestHeaders::kUserAgent));
}

TEST_F(URLProvisionFetcherTest, RejectsNonHttpsUrlWhenHardened) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(media::kHardenUrlProvisionFetcher);

  base::RunLoop run_loop;
  bool called = false;
  bool result = true;
  fetcher_->Retrieve(GURL("http://insecure.com"), kTestRequestBody,
                     base::BindOnce(
                         [](bool* called, bool* result, bool success,
                            const std::string& response) {
                           *called = true;
                           *result = success;
                         },
                         &called, &result)
                         .Then(run_loop.QuitClosure()));

  run_loop.Run();
  EXPECT_TRUE(called);
  EXPECT_FALSE(result);
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
}

TEST_F(URLProvisionFetcherTest, UnsecureRedirectsDisabledWhenHardened) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(media::kHardenUrlProvisionFetcher);

  base::HistogramTester histogram_tester;
  base::RunLoop run_loop;
  bool called = false;
  bool result = true;
  std::string received_response;

  const GURL kRequestedUrl("https://source.test/provision");
  const GURL kInsecureRedirectUrl("http://destination.test/provision");
  const std::string kResponsePayload = "redirected_response_payload";

  fetcher_->Retrieve(kRequestedUrl, kTestRequestBody,
                     base::BindOnce(
                         [](bool* called, bool* result, std::string* resp_out,
                            bool success, const std::string& response) {
                           *called = true;
                           *result = success;
                           *resp_out = response;
                         },
                         &called, &result, &received_response)
                         .Then(run_loop.QuitClosure()));

  // Configure an insecure (HTTP) redirect response:
  net::RedirectInfo redirect_info;
  redirect_info.status_code = 302;
  redirect_info.new_method = "POST";
  redirect_info.new_url = kInsecureRedirectUrl;

  network::TestURLLoaderFactory::Redirects redirects;
  redirects.emplace_back(redirect_info, network::mojom::URLResponseHead::New());

  // Simulate an insecure redirect attempt followed by the response payload:
  test_url_loader_factory_.AddResponse(
      kRequestedUrl, network::mojom::URLResponseHead::New(), kResponsePayload,
      network::URLLoaderCompletionStatus(net::OK), std::move(redirects));

  run_loop.Run();

  EXPECT_TRUE(called);
  EXPECT_FALSE(result);
  EXPECT_TRUE(received_response.empty());
  histogram_tester.ExpectUniqueSample(
      "Media.EME.UrlProvisionFetcher.ResponseCode", net::ERR_UNSAFE_REDIRECT,
      1);
}

TEST_F(URLProvisionFetcherTest, FollowsSecureRedirectWhenHardened) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(media::kHardenUrlProvisionFetcher);

  base::RunLoop run_loop;
  bool called = false;
  bool result = false;
  std::string received_response;

  const GURL kRequestedUrl("https://source.test/provision");
  const GURL kSecureRedirectUrl("https://destination.test/provision");
  const std::string kResponsePayload = "redirected_response_payload";

  fetcher_->Retrieve(kRequestedUrl, kTestRequestBody,
                     base::BindOnce(
                         [](bool* called, bool* result, std::string* resp_out,
                            bool success, const std::string& response) {
                           *called = true;
                           *result = success;
                           *resp_out = response;
                         },
                         &called, &result, &received_response)
                         .Then(run_loop.QuitClosure()));

  // Configure a secure 307 Redirect response preserving POST method:
  net::RedirectInfo redirect_info;
  redirect_info.status_code = 307;
  redirect_info.new_method = "POST";
  redirect_info.new_url = kSecureRedirectUrl;

  network::TestURLLoaderFactory::Redirects redirects;
  redirects.emplace_back(redirect_info, network::mojom::URLResponseHead::New());

  // Simulate a secure redirect followed by the response payload:
  test_url_loader_factory_.AddResponse(
      kRequestedUrl, network::mojom::URLResponseHead::New(), kResponsePayload,
      network::URLLoaderCompletionStatus(net::OK), std::move(redirects));

  run_loop.Run();

  EXPECT_TRUE(called);
  EXPECT_TRUE(result);
  EXPECT_EQ(received_response, kResponsePayload);
}

TEST_F(URLProvisionFetcherTest, RejectsNonPostRedirectWhenHardened) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(media::kHardenUrlProvisionFetcher);

  base::HistogramTester histogram_tester;
  base::RunLoop run_loop;
  bool called = false;
  bool result = true;
  std::string received_response;

  const GURL kRequestedUrl("https://source.test/provision");
  const GURL kSecureRedirectUrl("https://destination.test/provision");
  const std::string kResponsePayload = "redirected_response_payload";

  fetcher_->Retrieve(kRequestedUrl, kTestRequestBody,
                     base::BindOnce(
                         [](bool* called, bool* result, std::string* resp_out,
                            bool success, const std::string& response) {
                           *called = true;
                           *result = success;
                           *resp_out = response;
                         },
                         &called, &result, &received_response)
                         .Then(run_loop.QuitClosure()));

  // Configure a redirect where the method changes to GET (e.g. 302/303):
  net::RedirectInfo redirect_info;
  redirect_info.status_code = 303;
  redirect_info.new_method = "GET";
  redirect_info.new_url = kSecureRedirectUrl;

  network::TestURLLoaderFactory::Redirects redirects;
  redirects.emplace_back(redirect_info, network::mojom::URLResponseHead::New());

  test_url_loader_factory_.AddResponse(
      kRequestedUrl, network::mojom::URLResponseHead::New(), kResponsePayload,
      network::URLLoaderCompletionStatus(net::OK), std::move(redirects));

  run_loop.Run();

  EXPECT_TRUE(called);
  EXPECT_FALSE(result);
  EXPECT_TRUE(received_response.empty());
  histogram_tester.ExpectUniqueSample(
      "Media.EME.UrlProvisionFetcher.ResponseCode", net::ERR_UNSAFE_REDIRECT,
      1);
}

TEST_F(URLProvisionFetcherTest, RejectsOversizedResponseWhenHardened) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(media::kHardenUrlProvisionFetcher);

  base::RunLoop run_loop;
  bool called = false;
  bool result = true;
  std::string received_response;

  const GURL test_url(kTestUrl);
  fetcher_->Retrieve(test_url, kTestRequestBody,
                     base::BindOnce(
                         [](bool* called, bool* result, std::string* resp_out,
                            bool success, const std::string& response) {
                           *called = true;
                           *result = success;
                           *resp_out = response;
                         },
                         &called, &result, &received_response)
                         .Then(run_loop.QuitClosure()));

  // 64 KiB + 1 byte exceeds kMaxProvisionResponseSize (64 KiB).
  std::string oversized_payload(64 * 1024 + 1, 'a');
  test_url_loader_factory_.AddResponse(test_url.spec(), oversized_payload);

  run_loop.Run();

  EXPECT_TRUE(called);
  EXPECT_FALSE(result);
  EXPECT_TRUE(received_response.empty());
}

TEST_F(URLProvisionFetcherTest, AllowsHttpWhenHardeningDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(media::kHardenUrlProvisionFetcher);

  const GURL http_url("http://test.com");
  fetcher_->Retrieve(http_url, kTestRequestBody,
                     base::BindOnce([](bool, const std::string&) {}));

  EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);
  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
  EXPECT_EQ(pending_request->request.url, http_url);
}

}  // namespace content
