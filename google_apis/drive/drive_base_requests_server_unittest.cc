// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/drive/drive_base_requests.h"

#include <memory>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "google_apis/common/dummy_auth_service.h"
#include "google_apis/common/request_sender.h"
#include "google_apis/common/task_util.h"
#include "google_apis/common/test_util.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/test/fake_test_cert_verifier_params_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace google_apis {

namespace {

const char kTestUserAgent[] = "test-user-agent";

}  // namespace

class BaseRequestsServerTest : public testing::Test {
 protected:
  BaseRequestsServerTest() {
    mojo::Remote<network::mojom::NetworkService> network_service_remote;
    network_service_ = network::NetworkService::Create(
        network_service_remote.BindNewPipeAndPassReceiver());
    network::mojom::NetworkContextParamsPtr context_params =
        network::mojom::NetworkContextParams::New();
    // Use a dummy CertVerifier that always passes cert verification, since
    // these unittests don't need to test CertVerifier behavior.
    context_params->cert_verifier_params =
        network::FakeTestCertVerifierParamsFactory::GetCertVerifierParams();
    network_service_remote->CreateNetworkContext(
        network_context_.BindNewPipeAndPassReceiver(),
        std::move(context_params));

    mojo::PendingReceiver<network::mojom::URLLoaderNetworkServiceObserver>
        default_observer_receiver;
    network::mojom::NetworkServiceParamsPtr network_service_params =
        network::mojom::NetworkServiceParams::New();
    network_service_params->default_observer =
        default_observer_receiver.InitWithNewPipeAndPassRemote();
    network_service_remote->SetParams(std::move(network_service_params));

    network::mojom::URLLoaderFactoryParamsPtr params =
        network::mojom::URLLoaderFactoryParams::New();
    params->process_id = network::mojom::kBrowserProcessId;
    params->is_orb_enabled = false;
    network_context_->CreateURLLoaderFactory(
        url_loader_factory_.BindNewPipeAndPassReceiver(), std::move(params));
    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            url_loader_factory_.get());
  }

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    request_sender_ = std::make_unique<RequestSender>(
        std::make_unique<DummyAuthService>(), test_shared_loader_factory_,
        task_environment_.GetMainThreadTaskRunner(), kTestUserAgent,
        TRAFFIC_ANNOTATION_FOR_TESTS);

    ASSERT_TRUE(test_server_.InitializeAndListen());
    test_server_.RegisterRequestHandler(base::BindRepeating(
        &test_util::HandleDownloadFileRequest, test_server_.base_url(),
        base::Unretained(&http_request_)));
    test_server_.StartAcceptingConnections();
  }

  // Returns a temporary file path suitable for storing the cache file.
  base::FilePath GetTestCachedFilePath(const base::FilePath& file_name) {
    return temp_dir_.GetPath().Append(file_name);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  net::EmbeddedTestServer test_server_;
  std::unique_ptr<network::mojom::NetworkService> network_service_;
  mojo::Remote<network::mojom::NetworkContext> network_context_;
  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory_;
  scoped_refptr<network::WeakWrapperSharedURLLoaderFactory>
      test_shared_loader_factory_;
  std::unique_ptr<RequestSender> request_sender_;
  base::ScopedTempDir temp_dir_;

  // The incoming HTTP request is saved so tests can verify the request
  // parameters like HTTP method (ex. some requests should use DELETE
  // instead of GET).
  net::test_server::HttpRequest http_request_;
};

TEST_F(BaseRequestsServerTest, DownloadFileRequest_ValidFile) {
  ApiErrorCode result_code = OTHER_ERROR;
  base::FilePath temp_file;
  {
    base::RunLoop run_loop;
    std::unique_ptr<DownloadFileRequestBase> request =
        std::make_unique<DownloadFileRequestBase>(
            request_sender_.get(),
            test_util::CreateQuitCallback(
                &run_loop,
                test_util::CreateCopyResultCallback(&result_code, &temp_file)),
            GetContentCallback(), ProgressCallback(),
            test_server_.GetURL("/files/drive/testfile.txt"),
            GetTestCachedFilePath(
                base::FilePath::FromUTF8Unsafe("cached_testfile.txt")));
    request_sender_->StartRequestWithAuthRetry(std::move(request));
    run_loop.Run();
  }

  std::string contents;
  base::ReadFileToString(temp_file, &contents);
  base::DeleteFile(temp_file);

  EXPECT_EQ(HTTP_SUCCESS, result_code);
  EXPECT_EQ(net::test_server::METHOD_GET, http_request_.method);
  EXPECT_EQ("/files/drive/testfile.txt", http_request_.relative_url);

  const base::FilePath expected_path =
      test_util::GetTestFilePath("drive/testfile.txt");
  std::string expected_contents;
  base::ReadFileToString(expected_path, &expected_contents);
  EXPECT_EQ(expected_contents, contents);
}

TEST_F(BaseRequestsServerTest, DownloadFileRequest_NonExistentFile) {
  ApiErrorCode result_code = OTHER_ERROR;
  base::FilePath temp_file;
  {
    base::RunLoop run_loop;
    std::unique_ptr<DownloadFileRequestBase> request =
        std::make_unique<DownloadFileRequestBase>(
            request_sender_.get(),
            test_util::CreateQuitCallback(
                &run_loop,
                test_util::CreateCopyResultCallback(&result_code, &temp_file)),
            GetContentCallback(), ProgressCallback(),
            test_server_.GetURL("/files/gdata/no-such-file.txt"),
            GetTestCachedFilePath(
                base::FilePath::FromUTF8Unsafe("cache_no-such-file.txt")));
    request_sender_->StartRequestWithAuthRetry(std::move(request));
    run_loop.Run();
  }
  EXPECT_EQ(HTTP_NOT_FOUND, result_code);
  EXPECT_EQ(net::test_server::METHOD_GET, http_request_.method);
  EXPECT_EQ("/files/gdata/no-such-file.txt", http_request_.relative_url);
  // Do not verify the not found message.
}

}  // namespace google_apis
