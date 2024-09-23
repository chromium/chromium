// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/file_url_loader_factory.h"

#include <memory>
#include <string>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "components/file_access/scoped_file_access.h"
#include "components/file_access/scoped_file_access_delegate.h"
#include "components/file_access/test/mock_scoped_file_access_delegate.h"
#include "content/public/browser/file_url_loader.h"
#include "content/public/browser/shared_cors_origin_access_list.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/simple_url_loader_test_helper.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/filename_util.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/cors/origin_access_list.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/test/mock_url_loader_client.h"
#include "services/network/test/test_url_loader_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

using testing::_;

namespace {

// Implementation of SharedCorsOriginAccessList that is used by some of the unit
// tests below for verifying that FileURLLoaderFactory correctly grants or
// denies CORS exemptions to specific initiator origins.
//
// The implementation below gives https://www.google.com origin access to all
// file:// URLs.
class SharedCorsOriginAccessListForTesting : public SharedCorsOriginAccessList {
 public:
  SharedCorsOriginAccessListForTesting()
      : permitted_source_origin_(
            url::Origin::Create(GURL("https://www.google.com"))) {
    const std::string kFileProtocol("file");
    const std::string kAnyDomain;
    constexpr uint16_t kAnyPort = 0;
    list_.AddAllowListEntryForOrigin(
        permitted_source_origin_, kFileProtocol, kAnyDomain, kAnyPort,
        network::mojom::CorsDomainMatchMode::kDisallowSubdomains,
        network::mojom::CorsPortMatchMode::kAllowAnyPort,
        network::mojom::CorsOriginAccessMatchPriority::kDefaultPriority);
  }
  SharedCorsOriginAccessListForTesting(
      const SharedCorsOriginAccessListForTesting&) = delete;
  SharedCorsOriginAccessListForTesting& operator=(
      const SharedCorsOriginAccessListForTesting&) = delete;

  void SetForOrigin(
      const url::Origin& source_origin,
      std::vector<network::mojom::CorsOriginPatternPtr> allow_patterns,
      std::vector<network::mojom::CorsOriginPatternPtr> block_patterns,
      base::OnceClosure closure) override {}
  const network::cors::OriginAccessList& GetOriginAccessList() override {
    return list_;
  }

  url::Origin GetPermittedSourceOrigin() { return permitted_source_origin_; }

 private:
  ~SharedCorsOriginAccessListForTesting() override = default;

  network::cors::OriginAccessList list_;
  const url::Origin permitted_source_origin_;
};

GURL GetTestURL(const std::string& filename) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath path;
  base::PathService::Get(DIR_TEST_DATA, &path);
  path = path.AppendASCII("loader");
  return net::FilePathToFileURL(path.AppendASCII(filename));
}

class FileURLLoaderFactoryTest : public testing::Test {
 public:
  FileURLLoaderFactoryTest()
      : access_list_(
            base::MakeRefCounted<SharedCorsOriginAccessListForTesting>()) {
    factory_.Bind(FileURLLoaderFactory::Create(
        profile_dummy_path_, access_list_, base::TaskPriority::BEST_EFFORT));
  }
  FileURLLoaderFactoryTest(const FileURLLoaderFactoryTest&) = delete;
  FileURLLoaderFactoryTest& operator=(const FileURLLoaderFactoryTest&) = delete;
  ~FileURLLoaderFactoryTest() override = default;

 protected:
  int CreateLoaderAndRun(std::unique_ptr<network::ResourceRequest> request) {
    auto loader = network::SimpleURLLoader::Create(
        std::move(request), TRAFFIC_ANNOTATION_FOR_TESTS);

    SimpleURLLoaderTestHelper helper;
    loader->DownloadToString(
        factory_.get(), helper.GetCallbackDeprecated(),
        network::SimpleURLLoader::kMaxBoundedStringDownloadSize);

    helper.WaitForCallback();
    if (loader->ResponseInfo()) {
      response_info_ = loader->ResponseInfo()->Clone();
    }
    return loader->NetError();
  }

  ::network::URLLoaderCompletionStatus CreateLoaderBypassingSecurityAndRun(
      std::unique_ptr<network::ResourceRequest> request) {
    mojo::Receiver<network::mojom::URLLoaderClient> client_receiver{&client_};
    base::test::TestFuture<::network::URLLoaderCompletionStatus> future;
    content::CreateFileURLLoaderBypassingSecurityChecks(
        *request, loader_.BindNewPipeAndPassReceiver(),
        client_receiver.BindNewPipeAndPassRemote(),
        /*observer*/ nullptr,
        /*allow_directory_listing*/ true);
    EXPECT_CALL(client_, OnComplete)
        .WillOnce([&future](::network::URLLoaderCompletionStatus st) {
          future.SetValue(st);
        });
    return future.Get();
  }

  std::unique_ptr<network::ResourceRequest> CreateRequestWithMode(
      network::mojom::RequestMode request_mode) {
    auto request = std::make_unique<network::ResourceRequest>();
    request->url = GetTestURL("get.txt");
    request->mode = request_mode;
    return request;
  }

  std::unique_ptr<network::ResourceRequest> CreateCorsRequestWithInitiator(
      const url::Origin initiator_origin) {
    auto request = std::make_unique<network::ResourceRequest>();
    request->url = GetTestURL("get.txt");
    request->mode = network::mojom::RequestMode::kCors;
    request->request_initiator = initiator_origin;
    return request;
  }

  url::Origin GetPermittedSourceOrigin() {
    return access_list_->GetPermittedSourceOrigin();
  }

  network::mojom::URLResponseHead* ResponseInfo() {
    return response_info_.get();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  base::FilePath profile_dummy_path_;
  scoped_refptr<SharedCorsOriginAccessListForTesting> access_list_;
  mojo::Remote<network::mojom::URLLoaderFactory> factory_;
  network::mojom::URLResponseHeadPtr response_info_;
  testing::NiceMock<network::MockURLLoaderClient> client_;
  mojo::Remote<network::mojom::URLLoader> loader_;
};

TEST_F(FileURLLoaderFactoryTest, LastModified) {
  // The Last-Modified response header should be populated with the file
  // modification time.
  const char kTimeString[] = "Tue, 15 Nov 1994 12:45:26 GMT";

  // Create a temporary file with an arbitrary last-modified timestamp.
  base::FilePath file;
  ASSERT_TRUE(base::CreateTemporaryFile(&file));
  base::Time time;
  ASSERT_TRUE(base::Time::FromString(kTimeString, &time));
  ASSERT_TRUE(base::TouchFile(file, /*last_accessed=*/base::Time::Now(),
                              /*last_modified=*/time));

  // Request the file and extract the Last-Modified header.
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = net::FilePathToFileURL(file);
  std::string last_modified;
  ASSERT_EQ(net::OK, CreateLoaderAndRun(std::move(request)));
  ASSERT_NE(ResponseInfo(), nullptr);
  ASSERT_TRUE(ResponseInfo()->headers->EnumerateHeader(
      /*iter*/ nullptr, net::HttpResponseHeaders::kLastModified,
      &last_modified));

  // The header matches the file modification time.
  ASSERT_EQ(kTimeString, last_modified);
}

TEST_F(FileURLLoaderFactoryTest, Status) {
  base::FilePath file;
  ASSERT_TRUE(base::CreateTemporaryFile(&file));
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = net::FilePathToFileURL(file);
  ASSERT_EQ(net::OK, CreateLoaderAndRun(std::move(request)));

  ASSERT_NE(ResponseInfo(), nullptr);
  ASSERT_NE(ResponseInfo()->headers, nullptr);
  ASSERT_EQ(200, ResponseInfo()->headers->response_code());
  ASSERT_EQ("OK", ResponseInfo()->headers->GetStatusText());
}

TEST_F(FileURLLoaderFactoryTest, MissedRequestInitiator) {
  // CORS-disabled requests can omit |request.request_initiator| though it is
  // discouraged not to set |request.request_initiator|.
  EXPECT_EQ(net::OK, CreateLoaderAndRun(CreateRequestWithMode(
                         network::mojom::RequestMode::kSameOrigin)));

  EXPECT_EQ(net::OK, CreateLoaderAndRun(CreateRequestWithMode(
                         network::mojom::RequestMode::kNoCors)));

  EXPECT_EQ(net::OK, CreateLoaderAndRun(CreateRequestWithMode(
                         network::mojom::RequestMode::kNavigate)));

  // CORS-enabled requests need |request.request_initiator| set.
  EXPECT_EQ(net::ERR_INVALID_ARGUMENT,
            CreateLoaderAndRun(
                CreateRequestWithMode(network::mojom::RequestMode::kCors)));

  EXPECT_EQ(net::ERR_INVALID_ARGUMENT,
            CreateLoaderAndRun(CreateRequestWithMode(
                network::mojom::RequestMode::kCorsWithForcedPreflight)));
}

// Verify that FileURLLoaderFactory takes OriginAccessList into account when
// deciding whether to exempt a request from CORS.  See also
// https://crbug.com/1049604.
TEST_F(FileURLLoaderFactoryTest, Allowlist) {
  const url::Origin not_permitted_origin =
      url::Origin::Create(GURL("https://www.example.com"));

  // Request should fail unless the origin pair is not registered in the
  // allowlist.
  EXPECT_EQ(
      net::ERR_FAILED,
      CreateLoaderAndRun(CreateCorsRequestWithInitiator(not_permitted_origin)));

  // Registered access should pass.
  EXPECT_EQ(net::OK, CreateLoaderAndRun(CreateCorsRequestWithInitiator(
                         GetPermittedSourceOrigin())));

  // Isolated world origin should *not* be used to check the permission.
  auto request = CreateCorsRequestWithInitiator(not_permitted_origin);
  request->isolated_world_origin = GetPermittedSourceOrigin();
  EXPECT_EQ(net::ERR_FAILED, CreateLoaderAndRun(std::move(request)));
}

// Test that response type is set correctly for directory listings. Regression
// test for https://crbug.com/41492103.
TEST_F(FileURLLoaderFactoryTest, ResponseTypeForDirectoryListings) {
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  auto request = std::make_unique<network::ResourceRequest>();
  request->url =
      net::FilePathToFileURL(dir.GetPath().StripTrailingSeparators());
  request->mode = network::mojom::RequestMode::kNoCors;
  ASSERT_EQ(net::OK, CreateLoaderAndRun(std::move(request)));

  ASSERT_NE(ResponseInfo(), nullptr);
  EXPECT_EQ(network::mojom::FetchResponseType::kOpaque,
            ResponseInfo()->response_type);
}

}  // namespace

TEST_F(FileURLLoaderFactoryTest, DlpInitiatorAllow) {
  file_access::MockScopedFileAccessDelegate file_access_delegate;
  auto origin = url::Origin::Create(GURL("https://example.com"));

  EXPECT_CALL(file_access_delegate, RequestFilesAccess)
      .WillOnce(base::test::RunOnceCallback<2>(
          file_access::ScopedFileAccess::Allowed()));

  base::FilePath file;
  ASSERT_TRUE(base::CreateTemporaryFile(&file));
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = net::FilePathToFileURL(file);
  request->request_initiator = origin;
  ASSERT_EQ(net::OK, CreateLoaderAndRun(std::move(request)));

  ASSERT_NE(ResponseInfo(), nullptr);
  ASSERT_NE(ResponseInfo()->headers, nullptr);
  ASSERT_EQ(200, ResponseInfo()->headers->response_code());
  ASSERT_EQ("OK", ResponseInfo()->headers->GetStatusText());
}

TEST_F(FileURLLoaderFactoryTest, DlpNoInitiatorAllow) {
  file_access::MockScopedFileAccessDelegate file_access_delegate;

  EXPECT_CALL(file_access_delegate, RequestFilesAccessForSystem)
      .WillOnce(base::test::RunOnceCallback<1>(
          file_access::ScopedFileAccess::Allowed()));

  base::FilePath file;
  ASSERT_TRUE(base::CreateTemporaryFile(&file));
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = net::FilePathToFileURL(file);
  ASSERT_EQ(net::OK, CreateLoaderAndRun(std::move(request)));

  ASSERT_NE(ResponseInfo(), nullptr);
  ASSERT_NE(ResponseInfo()->headers, nullptr);
  ASSERT_EQ(200, ResponseInfo()->headers->response_code());
  ASSERT_EQ("OK", ResponseInfo()->headers->GetStatusText());
}

TEST_F(FileURLLoaderFactoryTest, DlpInitiatorDeny) {
  file_access::MockScopedFileAccessDelegate file_access_delegate;
  auto origin = url::Origin::Create(GURL("https://example.com"));

  EXPECT_CALL(file_access_delegate, RequestFilesAccess)
      .WillOnce(base::test::RunOnceCallback<2>(
          file_access::ScopedFileAccess::Denied()));

  base::FilePath file;
  ASSERT_TRUE(base::CreateTemporaryFile(&file));
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = net::FilePathToFileURL(file);
  request->request_initiator = origin;
  ASSERT_EQ(net::ERR_FAILED, CreateLoaderAndRun(std::move(request)));
}

TEST_F(FileURLLoaderFactoryTest, DlpNoInitiatorDeny) {
  file_access::MockScopedFileAccessDelegate file_access_delegate;

  EXPECT_CALL(file_access_delegate, RequestFilesAccessForSystem)
      .WillOnce(base::test::RunOnceCallback<1>(
          file_access::ScopedFileAccess::Denied()));

  base::FilePath file;
  ASSERT_TRUE(base::CreateTemporaryFile(&file));
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = net::FilePathToFileURL(file);
  ASSERT_EQ(net::ERR_FAILED, CreateLoaderAndRun(std::move(request)));
}

TEST_F(FileURLLoaderFactoryTest, DlpRemoteValidFileUrl) {
  file_access::MockScopedFileAccessDelegate file_access_delegate;
  base::MockCallback<base::RepeatingCallback<void(
      const std::vector<base::FilePath>&,
      base::OnceCallback<void(file_access::ScopedFileAccess)>)>>
      cb;
  EXPECT_CALL(cb, Run).WillOnce(
      base::test::RunOnceCallback<1>(file_access::ScopedFileAccess::Denied()));
  file_access::ScopedFileAccessDelegate::
      ScopedRequestFilesAccessCallbackForTesting scoped_callback(cb.Get());
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL("file:///test");

  EXPECT_EQ(net::ERR_FAILED,
            CreateLoaderBypassingSecurityAndRun(std::move(request)).error_code);
}

TEST_F(FileURLLoaderFactoryTest, DlpRemoteInvalidFileUrl) {
  file_access::MockScopedFileAccessDelegate file_access_delegate;
  base::MockCallback<base::RepeatingCallback<void(
      const std::vector<base::FilePath>&,
      base::OnceCallback<void(file_access::ScopedFileAccess)>)>>
      cb;
  EXPECT_CALL(cb, Run).Times(0);
  file_access::ScopedFileAccessDelegate::
      ScopedRequestFilesAccessCallbackForTesting scoped_callback(cb.Get());
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL("file:///%00");

  EXPECT_EQ(net::ERR_INVALID_URL,
            CreateLoaderBypassingSecurityAndRun(std::move(request)).error_code);
}

}  // namespace content
