// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/url_loader/navigation_body_loader.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/cert/x509_util.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/test/cert_test_util.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/navigation/navigation_params.h"
#include "third_party/blink/public/mojom/navigation/navigation_params.mojom.h"
#include "third_party/blink/public/platform/resource_load_info_notifier_wrapper.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_navigation_body_loader.h"
#include "third_party/blink/public/web/web_navigation_params.h"
#include "third_party/blink/renderer/platform/loader/fetch/code_cache_host.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/weborigin/referrer.h"

namespace blink {

namespace {

class FakeCodeCacheHost : public mojom::CodeCacheHost {
 public:
  FakeCodeCacheHost()
      : code_cache_host_(mojo::Remote<mojom::CodeCacheHost>(
            receiver_.BindNewPipeAndPassRemote())) {}

  // blink::mojom::CodeCacheHost implementation.
  void DidGenerateCacheableMetadata(blink::mojom::CodeCacheType cache_type,
                                    const GURL& url,
                                    base::Time expected_response_time,
                                    mojo_base::BigBuffer data) override {}
  void FetchCachedCode(blink::mojom::CodeCacheType cache_type,
                       const GURL& url,
                       FetchCachedCodeCallback callback) override {
    if (run_loop_.running())
      run_loop_.Quit();
    callback_ = std::move(callback);
  }
  void ClearCodeCacheEntry(blink::mojom::CodeCacheType cache_type,
                           const GURL& url) override {}
  void DidGenerateCacheableMetadataInCacheStorage(
      const GURL& url,
      base::Time expected_response_time,
      mojo_base::BigBuffer data,
      const url::Origin& cache_storage_origin,
      const std::string& cache_storage_cache_name) override {}

  blink::CodeCacheHost* GetCodeCacheHost() { return &code_cache_host_; }

  void FinishFetch() {
    if (!callback_)
      run_loop_.Run();
    std::move(callback_).Run(base::Time(), std::vector<uint8_t>());
  }

 private:
  FetchCachedCodeCallback callback_;
  mojo::Receiver<mojom::CodeCacheHost> receiver_{this};
  blink::CodeCacheHost code_cache_host_;
  base::RunLoop run_loop_;
};

class NavigationBodyLoaderTest : public ::testing::Test,
                                 public WebNavigationBodyLoader::Client {
 protected:
  NavigationBodyLoaderTest() {}

  ~NavigationBodyLoaderTest() override { base::RunLoop().RunUntilIdle(); }

  MojoCreateDataPipeOptions CreateDataPipeOptions() {
    MojoCreateDataPipeOptions options;
    options.struct_size = sizeof(MojoCreateDataPipeOptions);
    options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
    options.element_num_bytes = 1;
    options.capacity_num_bytes = 1024;
    return options;
  }

  void CreateBodyLoader() {
    mojo::ScopedDataPipeProducerHandle producer_handle;
    mojo::ScopedDataPipeConsumerHandle consumer_handle;
    MojoCreateDataPipeOptions options = CreateDataPipeOptions();
    ASSERT_EQ(mojo::CreateDataPipe(&options, producer_handle, consumer_handle),
              MOJO_RESULT_OK);

    writer_ = std::move(producer_handle);
    auto endpoints = network::mojom::URLLoaderClientEndpoints::New();
    endpoints->url_loader_client = client_remote_.BindNewPipeAndPassReceiver();
    WebNavigationParams navigation_params;
    auto common_params = CreateCommonNavigationParams();
    common_params->request_destination =
        network::mojom::RequestDestination::kDocument;
    auto commit_params = CreateCommitNavigationParams();
    WebNavigationBodyLoader::FillNavigationParamsResponseAndBodyLoader(
        std::move(common_params), std::move(commit_params), /*request_id=*/1,
        network::mojom::URLResponseHead::New(), std::move(consumer_handle),
        std::move(endpoints), scheduler::GetSingleThreadTaskRunnerForTesting(),
        std::make_unique<ResourceLoadInfoNotifierWrapper>(
            /*resource_load_info_notifier=*/nullptr),
        /*is_main_frame=*/true, &navigation_params);
    loader_ = std::move(navigation_params.body_loader);
  }

  void StartLoading(CodeCacheHost* code_cache_host = nullptr) {
    loader_->StartLoadingBody(this, code_cache_host);
    base::RunLoop().RunUntilIdle();
  }

  void Write(const std::string& buffer) {
    uint32_t size = static_cast<uint32_t>(buffer.size());
    MojoResult result = writer_->WriteData(buffer.c_str(), &size, kNone);
    ASSERT_EQ(MOJO_RESULT_OK, result);
    ASSERT_EQ(buffer.size(), size);
  }

  void Complete(int net_error) {
    client_remote_->OnComplete(network::URLLoaderCompletionStatus(net_error));
    base::RunLoop().RunUntilIdle();
  }

  void BodyCodeCacheReceived(mojo_base::BigBuffer data) override {
    ASSERT_TRUE(expecting_code_cache_received_);
    did_receive_code_cache_ = true;
    if (run_loop_)
      run_loop_->Quit();
  }

  void BodyDataReceived(base::span<const char> data) override {
    ASSERT_TRUE(expecting_data_received_);
    did_receive_data_ = true;
    data_received_ += std::string(data.data(), data.size());
    TakeActions();
    if (run_loop_)
      run_loop_->Quit();
  }

  void BodyLoadingFinished(base::TimeTicks completion_time,
                           int64_t total_encoded_data_length,
                           int64_t total_encoded_body_length,
                           int64_t total_decoded_body_length,
                           bool should_report_corb_blocking,
                           const absl::optional<WebURLError>& error) override {
    ASSERT_TRUE(expecting_finished_);
    did_finish_ = true;
    error_ = error;
    TakeActions();
    if (run_loop_)
      run_loop_->Quit();
  }

  void TakeActions() {
    if (!buffer_to_write_.empty()) {
      std::string buffer = buffer_to_write_;
      buffer_to_write_ = std::string();
      ExpectDataReceived();
      Write(buffer);
    }
    if (toggle_defers_loading_) {
      toggle_defers_loading_ = false;
      loader_->SetDefersLoading(WebLoaderFreezeMode::kNone);
      loader_->SetDefersLoading(WebLoaderFreezeMode::kStrict);
    }
    if (destroy_loader_) {
      destroy_loader_ = false;
      loader_.reset();
    }
  }

  void ExpectCodeCacheReceived() {
    expecting_code_cache_received_ = true;
    did_receive_code_cache_ = false;
  }

  void ExpectDataReceived() {
    expecting_data_received_ = true;
    did_receive_data_ = false;
  }

  void ExpectFinished() {
    expecting_finished_ = true;
    did_finish_ = false;
  }

  std::string TakeDataReceived() {
    std::string data = data_received_;
    data_received_ = std::string();
    return data;
  }

  void Wait() {
    if (expecting_code_cache_received_) {
      if (!did_receive_code_cache_)
        WaitForRunLoop();
      ASSERT_TRUE(did_receive_code_cache_);
      expecting_code_cache_received_ = false;
    }
    if (expecting_data_received_) {
      if (!did_receive_data_)
        WaitForRunLoop();
      ASSERT_TRUE(did_receive_data_);
      expecting_data_received_ = false;
    }
    if (expecting_finished_) {
      if (!did_finish_)
        WaitForRunLoop();
      ASSERT_TRUE(did_finish_);
      expecting_finished_ = false;
    }
  }

  void WaitForRunLoop() {
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
    run_loop_.reset();
  }

  base::test::TaskEnvironment task_environment_;
  static const MojoWriteDataFlags kNone = MOJO_WRITE_DATA_FLAG_NONE;
  mojo::Remote<network::mojom::URLLoaderClient> client_remote_;
  std::unique_ptr<WebNavigationBodyLoader> loader_;
  mojo::ScopedDataPipeProducerHandle writer_;

  std::unique_ptr<base::RunLoop> run_loop_;
  bool expecting_data_received_ = false;
  bool did_receive_data_ = false;
  bool expecting_finished_ = false;
  bool did_finish_ = false;
  // Code cache is always called by default.
  bool expecting_code_cache_received_ = true;
  bool did_receive_code_cache_ = false;
  std::string buffer_to_write_;
  bool toggle_defers_loading_ = false;
  bool destroy_loader_ = false;
  std::string data_received_;
  absl::optional<WebURLError> error_;
};

TEST_F(NavigationBodyLoaderTest, SetDefersBeforeStart) {
  CreateBodyLoader();
  loader_->SetDefersLoading(WebLoaderFreezeMode::kStrict);
  loader_->SetDefersLoading(WebLoaderFreezeMode::kNone);
  // Should not crash.
}

TEST_F(NavigationBodyLoaderTest, DataReceived) {
  CreateBodyLoader();
  StartLoading();
  ExpectDataReceived();
  Write("hello");
  Wait();
  EXPECT_EQ("hello", TakeDataReceived());
}

TEST_F(NavigationBodyLoaderTest, DataReceivedFromDataReceived) {
  CreateBodyLoader();
  StartLoading();
  ExpectDataReceived();
  buffer_to_write_ = "world";
  Write("hello");
  Wait();
  EXPECT_EQ("helloworld", TakeDataReceived());
}

TEST_F(NavigationBodyLoaderTest, DestroyFromDataReceived) {
  CreateBodyLoader();
  StartLoading();
  ExpectDataReceived();
  destroy_loader_ = false;
  Write("hello");
  Wait();
  EXPECT_EQ("hello", TakeDataReceived());
}

TEST_F(NavigationBodyLoaderTest, SetDefersLoadingFromDataReceived) {
  CreateBodyLoader();
  StartLoading();
  ExpectDataReceived();
  toggle_defers_loading_ = true;
  Write("hello");
  Wait();
  EXPECT_EQ("hello", TakeDataReceived());
}

TEST_F(NavigationBodyLoaderTest, StartDeferred) {
  CreateBodyLoader();
  loader_->SetDefersLoading(WebLoaderFreezeMode::kStrict);
  StartLoading();
  Write("hello");
  ExpectDataReceived();
  loader_->SetDefersLoading(WebLoaderFreezeMode::kNone);
  Wait();
  EXPECT_EQ("hello", TakeDataReceived());
}

TEST_F(NavigationBodyLoaderTest, StartDeferredWithBackForwardCache) {
  CreateBodyLoader();
  loader_->SetDefersLoading(WebLoaderFreezeMode::kBufferIncoming);
  StartLoading();
  Write("hello");
  ExpectDataReceived();
  loader_->SetDefersLoading(WebLoaderFreezeMode::kNone);
  Wait();
  EXPECT_EQ("hello", TakeDataReceived());
}

TEST_F(NavigationBodyLoaderTest, OnCompleteThenClose) {
  CreateBodyLoader();
  StartLoading();
  Complete(net::ERR_FAILED);
  ExpectFinished();
  writer_.reset();
  Wait();
  EXPECT_TRUE(error_.has_value());
}

TEST_F(NavigationBodyLoaderTest, DestroyFromOnCompleteThenClose) {
  CreateBodyLoader();
  StartLoading();
  Complete(net::ERR_FAILED);
  ExpectFinished();
  destroy_loader_ = true;
  writer_.reset();
  Wait();
  EXPECT_TRUE(error_.has_value());
}

TEST_F(NavigationBodyLoaderTest, SetDefersLoadingFromOnCompleteThenClose) {
  CreateBodyLoader();
  StartLoading();
  Complete(net::ERR_FAILED);
  ExpectFinished();
  toggle_defers_loading_ = true;
  writer_.reset();
  Wait();
  EXPECT_TRUE(error_.has_value());
}

TEST_F(NavigationBodyLoaderTest, CloseThenOnComplete) {
  CreateBodyLoader();
  StartLoading();
  writer_.reset();
  ExpectFinished();
  Complete(net::ERR_FAILED);
  Wait();
  EXPECT_TRUE(error_.has_value());
}

TEST_F(NavigationBodyLoaderTest, DestroyFromCloseThenOnComplete) {
  CreateBodyLoader();
  StartLoading();
  writer_.reset();
  ExpectFinished();
  destroy_loader_ = true;
  Complete(net::ERR_FAILED);
  Wait();
  EXPECT_TRUE(error_.has_value());
}

TEST_F(NavigationBodyLoaderTest, SetDefersLoadingFromCloseThenOnComplete) {
  CreateBodyLoader();
  StartLoading();
  writer_.reset();
  ExpectFinished();
  toggle_defers_loading_ = true;
  Complete(net::ERR_FAILED);
  Wait();
  EXPECT_TRUE(error_.has_value());
}

// Tests that FillNavigationParamsResponseAndBodyLoader populates security
// details on the response when they are present.
TEST_F(NavigationBodyLoaderTest, FillResponseWithSecurityDetails) {
  auto response = network::mojom::URLResponseHead::New();
  response->ssl_info = net::SSLInfo();
  net::CertificateList certs;
  ASSERT_TRUE(net::LoadCertificateFiles(
      {"subjectAltName_sanity_check.pem", "root_ca_cert.pem"}, &certs));
  ASSERT_EQ(2U, certs.size());

  base::StringPiece cert0_der =
      net::x509_util::CryptoBufferAsStringPiece(certs[0]->cert_buffer());
  base::StringPiece cert1_der =
      net::x509_util::CryptoBufferAsStringPiece(certs[1]->cert_buffer());

  response->ssl_info->cert =
      net::X509Certificate::CreateFromDERCertChain({cert0_der, cert1_der});
  net::SSLConnectionStatusSetVersion(net::SSL_CONNECTION_VERSION_TLS1_2,
                                     &response->ssl_info->connection_status);

  auto common_params = CreateCommonNavigationParams();
  common_params->url = GURL("https://example.test");
  common_params->request_destination =
      network::mojom::RequestDestination::kDocument;
  auto commit_params = CreateCommitNavigationParams();

  WebNavigationParams navigation_params;
  auto endpoints = network::mojom::URLLoaderClientEndpoints::New();
  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  MojoResult rv =
      mojo::CreateDataPipe(nullptr, producer_handle, consumer_handle);
  ASSERT_EQ(MOJO_RESULT_OK, rv);
  WebNavigationBodyLoader::FillNavigationParamsResponseAndBodyLoader(
      std::move(common_params), std::move(commit_params), /*request_id=*/1,
      std::move(response), std::move(consumer_handle), std::move(endpoints),
      scheduler::GetSingleThreadTaskRunnerForTesting(),
      std::make_unique<ResourceLoadInfoNotifierWrapper>(
          /*resource_load_info_notifier=*/nullptr),
      /*is_main_frame=*/true, &navigation_params);
  EXPECT_TRUE(
      navigation_params.response.ToResourceResponse().GetSSLInfo().has_value());
}

// Tests that FillNavigationParamsResponseAndBodyLoader populates referrer
// on redirects correctly.
TEST_F(NavigationBodyLoaderTest, FillResponseReferrerRedirects) {
  auto response = network::mojom::URLResponseHead::New();
  auto common_params = CreateCommonNavigationParams();
  common_params->url = GURL("https://example.test");
  common_params->request_destination =
      network::mojom::RequestDestination::kDocument;
  auto commit_params = CreateCommitNavigationParams();
  // The first redirect will have an empty referrer, which should result in an
  // output of the default WebString. The second has an actual referrer, which
  // should be populated verbatim.
  net::RedirectInfo first_redirect_info;
  net::RedirectInfo second_redirect_info;
  GURL first_redirect_url = GURL("");
  GURL second_redirect_url = GURL("https://www.google.com");
  second_redirect_info.new_referrer = second_redirect_url.spec();

  network::mojom::URLResponseHeadPtr first_redirect_response =
      network::mojom::URLResponseHead::New();
  network::mojom::URLResponseHeadPtr second_redirect_response =
      network::mojom::URLResponseHead::New();
  commit_params->redirect_infos.push_back(first_redirect_info);
  commit_params->redirect_infos.push_back(second_redirect_info);
  commit_params->redirect_response.push_back(
      std::move(first_redirect_response));
  commit_params->redirect_response.push_back(
      std::move(second_redirect_response));
  commit_params->redirects.push_back(first_redirect_url);
  commit_params->redirects.push_back(second_redirect_url);

  WebNavigationParams navigation_params;
  auto endpoints = network::mojom::URLLoaderClientEndpoints::New();
  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  MojoResult rv =
      mojo::CreateDataPipe(nullptr, producer_handle, consumer_handle);
  ASSERT_EQ(MOJO_RESULT_OK, rv);
  WebNavigationBodyLoader::FillNavigationParamsResponseAndBodyLoader(
      std::move(common_params), std::move(commit_params), /*request_id=*/1,
      std::move(response), std::move(consumer_handle), std::move(endpoints),
      scheduler::GetSingleThreadTaskRunnerForTesting(),
      std::make_unique<ResourceLoadInfoNotifierWrapper>(
          /*resource_load_info_notifier=*/nullptr),
      /*is_main_frame=*/true, &navigation_params);
  ASSERT_EQ(navigation_params.redirects.size(), 2u);
  ASSERT_EQ(navigation_params.redirects[0].new_referrer,
            WebString(Referrer::NoReferrer()));
  ASSERT_EQ(navigation_params.redirects[1].new_referrer,
            WebString::FromUTF8(second_redirect_url.spec()));
}

TEST_F(NavigationBodyLoaderTest, CodeCache) {
  FakeCodeCacheHost code_cache_host;
  CreateBodyLoader();
  StartLoading(code_cache_host.GetCodeCacheHost());
  code_cache_host.FinishFetch();
  ExpectDataReceived();
  Write("hello");
  Wait();
  EXPECT_EQ("hello", TakeDataReceived());
}

TEST_F(NavigationBodyLoaderTest, CodeCacheInParallelWithEarlyBodyLoad) {
  base::test::ScopedFeatureList feature_list(features::kEarlyBodyLoad);
  FakeCodeCacheHost code_cache_host;
  CreateBodyLoader();
  StartLoading(code_cache_host.GetCodeCacheHost());

  // We should be able to receive data without receiving the code cache.
  expecting_code_cache_received_ = false;
  ExpectDataReceived();
  Write("hello");
  Complete(net::OK);
  writer_.reset();
  Wait();
  EXPECT_EQ("hello", TakeDataReceived());

  // Now wait for the code cache, and loading should finish.
  ExpectCodeCacheReceived();
  ExpectFinished();
  code_cache_host.FinishFetch();
  Wait();
}

}  // namespace

}  // namespace blink
