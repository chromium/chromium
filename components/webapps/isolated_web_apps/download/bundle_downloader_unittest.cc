// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/isolated_web_apps/download/bundle_downloader.h"

#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {
namespace {

using ::testing::Eq;
using ::testing::IsFalse;
using ::testing::IsTrue;

class IsolatedWebAppDownloaderTest : public ::testing::Test {
 public:
  IsolatedWebAppDownloaderTest()
      : shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_factory_)) {}

  void SetUp() override { CHECK(temp_dir_.CreateUniqueTempDir()); }

  base::FilePath bundle_path() {
    return temp_dir_.GetPath().Append(FILE_PATH_LITERAL("bundle.swbn"));
  }

  GURL download_url() { return GURL("https://example.com/bundle.swbn"); }

 protected:
  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;

  base::ScopedTempDir temp_dir_;
};
}  // namespace

TEST_F(IsolatedWebAppDownloaderTest, SuccessfulDownload) {
  test_factory_.AddResponse(download_url().spec(), "test bundle content",
                            net::HttpStatusCode::HTTP_OK);

  base::test::TestFuture<int32_t> future;
  auto downloader = IsolatedWebAppDownloader::CreateAndStartDownloading(
      download_url(), bundle_path(), PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS,
      shared_url_loader_factory_,
      /*client_security_state_address_space=*/std::nullopt,
      future.GetCallback());

  EXPECT_THAT(future.Take(), Eq(net::OK));
  EXPECT_THAT(base::PathExists(bundle_path()), IsTrue());

  std::string file_contents;
  EXPECT_THAT(base::ReadFileToString(bundle_path(), &file_contents), IsTrue());
  EXPECT_THAT(file_contents, Eq("test bundle content"));
}

TEST_F(IsolatedWebAppDownloaderTest, FailedDownload) {
  test_factory_.AddResponse(download_url().spec(), "",
                            net::HttpStatusCode::HTTP_NOT_FOUND);

  base::test::TestFuture<int32_t> future;
  auto downloader = IsolatedWebAppDownloader::CreateAndStartDownloading(
      download_url(), bundle_path(), PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS,
      shared_url_loader_factory_,
      /*client_security_state_address_space=*/std::nullopt,
      future.GetCallback());

  EXPECT_THAT(future.Take(), Eq(net::Error::ERR_HTTP_RESPONSE_CODE_FAILURE));
  EXPECT_THAT(base::PathExists(bundle_path()), IsFalse());
}

TEST_F(IsolatedWebAppDownloaderTest,
       SuccessfulPartialDownloadServerIgnoresRange) {
  test_factory_.AddResponse(download_url().spec(), std::string(10 * 1024, 'x'),
                            net::HttpStatusCode::HTTP_OK);

  base::test::TestFuture<std::optional<std::string>> future;
  auto downloader =
      IsolatedWebAppDownloader::Create(shared_url_loader_factory_);
  downloader->DownloadInitialBytes(
      download_url(), PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS,
      /*client_security_state_address_space=*/std::nullopt,
      future.GetCallback());

  EXPECT_THAT(future.Take(), Eq(std::string(8 * 1024, 'x')));
}

TEST_F(IsolatedWebAppDownloaderTest, SuccessfulPartialDownload) {
  test_factory_.AddResponse(download_url().spec(), std::string(8 * 1024, 'x'),
                            net::HttpStatusCode::HTTP_PARTIAL_CONTENT);

  base::test::TestFuture<std::optional<std::string>> future;
  auto downloader =
      IsolatedWebAppDownloader::Create(shared_url_loader_factory_);
  downloader->DownloadInitialBytes(
      download_url(), PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS,
      /*client_security_state_address_space=*/std::nullopt,
      future.GetCallback());

  EXPECT_THAT(future.Take(), Eq(std::string(8 * 1024, 'x')));
}

TEST_F(IsolatedWebAppDownloaderTest, SuccessfulPartialDownloadOfSmallContent) {
  test_factory_.AddResponse(download_url().spec(), "cthulhu",
                            net::HttpStatusCode::HTTP_OK);

  base::test::TestFuture<std::optional<std::string>> future;
  auto downloader =
      IsolatedWebAppDownloader::Create(shared_url_loader_factory_);
  downloader->DownloadInitialBytes(
      download_url(), PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS,
      /*client_security_state_address_space=*/std::nullopt,
      future.GetCallback());

  EXPECT_THAT(future.Take(), Eq("cthulhu"));
}

TEST_F(IsolatedWebAppDownloaderTest,
       SetsNoClientSecurityStateWhenAddressSpaceOmitted) {
  auto downloader =
      IsolatedWebAppDownloader::Create(shared_url_loader_factory_);
  base::test::TestFuture<int32_t> future;
  downloader->DownloadSignedWebBundle(
      download_url(), bundle_path(), PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS,
      /*client_security_state_address_space=*/std::nullopt,
      future.GetCallback());
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return test_factory_.NumPending() > 0; }));

  ASSERT_EQ(test_factory_.NumPending(), 1);
  const network::ResourceRequest& request =
      test_factory_.GetPendingRequest(0)->request;

  EXPECT_TRUE(!request.trusted_params ||
              !request.trusted_params->client_security_state);
}

TEST_F(IsolatedWebAppDownloaderTest,
       SetsClientSecurityStateFromExplicitAddressSpace) {
  auto downloader =
      IsolatedWebAppDownloader::Create(shared_url_loader_factory_);
  base::test::TestFuture<int32_t> future;
  downloader->DownloadSignedWebBundle(
      download_url(), bundle_path(), PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS,
      network::mojom::IPAddressSpace::kPublic, future.GetCallback());
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return test_factory_.NumPending() > 0; }));

  ASSERT_EQ(test_factory_.NumPending(), 1);
  const network::ResourceRequest& request =
      test_factory_.GetPendingRequest(0)->request;

  ASSERT_TRUE(request.trusted_params);
  ASSERT_TRUE(request.trusted_params->client_security_state);
  EXPECT_EQ(request.trusted_params->client_security_state->ip_address_space,
            network::mojom::IPAddressSpace::kPublic);
  EXPECT_TRUE(
      request.trusted_params->client_security_state->is_web_secure_context);
  EXPECT_EQ(request.trusted_params->client_security_state
                ->local_network_access_request_policy,
            network::mojom::LocalNetworkAccessRequestPolicy::kBlock);
}

TEST_F(IsolatedWebAppDownloaderTest,
       SetsClientSecurityStateForLocalAddressSpace) {
  auto downloader =
      IsolatedWebAppDownloader::Create(shared_url_loader_factory_);
  base::test::TestFuture<int32_t> future;
  downloader->DownloadSignedWebBundle(
      download_url(), bundle_path(), PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS,
      network::mojom::IPAddressSpace::kLocal, future.GetCallback());
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return test_factory_.NumPending() > 0; }));

  ASSERT_EQ(test_factory_.NumPending(), 1);
  const network::ResourceRequest& request =
      test_factory_.GetPendingRequest(0)->request;

  ASSERT_TRUE(request.trusted_params);
  ASSERT_TRUE(request.trusted_params->client_security_state);
  EXPECT_EQ(request.trusted_params->client_security_state->ip_address_space,
            network::mojom::IPAddressSpace::kLocal);
}

TEST_F(IsolatedWebAppDownloaderTest,
       SetsClientSecurityStateForLoopbackAddressSpace) {
  auto downloader =
      IsolatedWebAppDownloader::Create(shared_url_loader_factory_);
  base::test::TestFuture<int32_t> future;
  downloader->DownloadSignedWebBundle(
      download_url(), bundle_path(), PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS,
      network::mojom::IPAddressSpace::kLoopback, future.GetCallback());
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return test_factory_.NumPending() > 0; }));

  ASSERT_EQ(test_factory_.NumPending(), 1);
  const network::ResourceRequest& request =
      test_factory_.GetPendingRequest(0)->request;

  ASSERT_TRUE(request.trusted_params);
  ASSERT_TRUE(request.trusted_params->client_security_state);
  EXPECT_EQ(request.trusted_params->client_security_state->ip_address_space,
            network::mojom::IPAddressSpace::kLoopback);
}

}  // namespace web_app
