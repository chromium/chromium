// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/url_provision_fetcher.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "content/public/browser/provision_fetcher_factory.h"
#include "media/base/media_switches.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

namespace {

constexpr char kTestUrl[] = "http://test.com";
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

TEST_F(URLProvisionFetcherTest,
       FeatureDisabled_SendsPostRequestWithParamInUrl) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(media::kUsePostBodyForUrlProvisionFetcher);

  const GURL expected_url(
      base::StrCat({GURL(kTestUrl).spec(), "&", kRequestParam}));

  base::RunLoop run_loop;
  fetcher_->Retrieve(GURL(kTestUrl), kTestRequestBody,
                     base::BindOnce([](bool, const std::string&) {
                     }).Then(run_loop.QuitClosure()));

  const network::TestURLLoaderFactory::PendingRequest* pending_request_ptr;
  CheckCommonRequestExpectations(expected_url, &pending_request_ptr);
  const auto& pending_request = pending_request_ptr->request;

  EXPECT_EQ(pending_request.url, expected_url);
  EXPECT_EQ(pending_request.headers.GetHeader("Content-Type"),
            "application/json");
  EXPECT_TRUE(pending_request.request_body->elements()->empty());

  SimulateResponse(expected_url.spec());
  run_loop.Run();
}

TEST_F(URLProvisionFetcherTest, FeatureEnabled_SendsPostRequestWithBody) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(media::kUsePostBodyForUrlProvisionFetcher);

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

}  // namespace content
