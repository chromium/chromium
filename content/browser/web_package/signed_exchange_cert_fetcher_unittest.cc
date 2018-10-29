// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_cert_fetcher.h"

#include "base/callback.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "base/test/scoped_task_environment.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/public/common/resource_type.h"
#include "content/public/common/url_loader_throttle.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "net/base/load_flags.h"
#include "net/cert/x509_util.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

class DeferringURLLoaderThrottle final : public URLLoaderThrottle {
 public:
  DeferringURLLoaderThrottle() = default;
  ~DeferringURLLoaderThrottle() override = default;

  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override {
    will_start_request_called_ = true;
    *defer = true;
  }

  void WillRedirectRequest(
      const net::RedirectInfo& /* redirect_info */,
      const network::ResourceResponseHead& /* response_head */,
      bool* defer,
      std::vector<std::string>* /* to_be_removed_headers */,
      net::HttpRequestHeaders* /* modified_headers */) override {
    will_redirect_request_called_ = true;
    *defer = true;
  }

  void WillProcessResponse(const GURL& response_url_,
                           network::ResourceResponseHead* response_head,
                           bool* defer) override {
    will_process_response_called_ = true;
    *defer = true;
  }

  bool will_start_request_called() const { return will_start_request_called_; }
  bool will_redirect_request_called() const {
    return will_redirect_request_called_;
  }
  bool will_process_response_called() const {
    return will_process_response_called_;
  }

  Delegate* delegate() { return delegate_; }

 private:
  bool will_start_request_called_ = false;
  bool will_redirect_request_called_ = false;
  bool will_process_response_called_ = false;

  DISALLOW_COPY_AND_ASSIGN(DeferringURLLoaderThrottle);
};

class MockURLLoader final : public network::mojom::URLLoader {
 public:
  MockURLLoader(network::mojom::URLLoaderRequest url_loader_request)
      : binding_(this, std::move(url_loader_request)) {}
  ~MockURLLoader() override = default;

  MOCK_METHOD2(FollowRedirect,
               void(const base::Optional<std::vector<std::string>>&,
                    const base::Optional<net::HttpRequestHeaders>&));
  MOCK_METHOD0(ProceedWithResponse, void());
  MOCK_METHOD2(SetPriority,
               void(net::RequestPriority priority,
                    int32_t intra_priority_value));
  MOCK_METHOD0(PauseReadingBodyFromNet, void());
  MOCK_METHOD0(ResumeReadingBodyFromNet, void());

 private:
  mojo::Binding<network::mojom::URLLoader> binding_;

  DISALLOW_COPY_AND_ASSIGN(MockURLLoader);
};

class URLLoaderFactoryForMockLoader final
    : public network::mojom::URLLoaderFactory {
 public:
  URLLoaderFactoryForMockLoader() = default;
  ~URLLoaderFactoryForMockLoader() override = default;

  // network::mojom::URLLoaderFactory implementation.
  void CreateLoaderAndStart(network::mojom::URLLoaderRequest url_loader_request,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const network::ResourceRequest& url_request,
                            network::mojom::URLLoaderClientPtr client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override {
    loader_ = std::make_unique<MockURLLoader>(std::move(url_loader_request));
    url_request_ = url_request;
    client_ptr_ = std::move(client);
  }

  void Clone(network::mojom::URLLoaderFactoryRequest factory) override {
    NOTREACHED();
  }

  network::mojom::URLLoaderClientPtr& client_ptr() { return client_ptr_; }
  void CloseClientPipe() { client_ptr_.reset(); }

  base::Optional<network::ResourceRequest> url_request() const {
    return url_request_;
  }

 private:
  std::unique_ptr<MockURLLoader> loader_;
  network::mojom::URLLoaderClientPtr client_ptr_;
  base::Optional<network::ResourceRequest> url_request_;

  DISALLOW_COPY_AND_ASSIGN(URLLoaderFactoryForMockLoader);
};

void ForwardCertificateCallback(
    bool* called,
    SignedExchangeLoadResult* out_result,
    std::unique_ptr<SignedExchangeCertificateChain>* out_cert,
    SignedExchangeLoadResult result,
    std::unique_ptr<SignedExchangeCertificateChain> cert_chain) {
  *out_result = result;
  *called = true;
  *out_cert = std::move(cert_chain);
}

class SignedExchangeCertFetcherTest : public testing::Test {
 public:
  SignedExchangeCertFetcherTest()
      : url_(GURL("https://www.example.com/cert")),
        request_initiator_(
            url::Origin::Create(GURL("https://sxg.example.com/test.sxg"))),
        resource_dispatcher_host_(CreateDownloadHandlerIntercept(),
                                  base::ThreadTaskRunnerHandle::Get(),
                                  true /* enable_resource_scheduler */) {}
  ~SignedExchangeCertFetcherTest() override {}

 protected:
  static scoped_refptr<net::X509Certificate> ImportTestCert() {
    return net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  }

  static std::string CreateCertMessage(const base::StringPiece& cert_data) {
    cbor::Value::MapValue cbor_map;
    cbor_map[cbor::Value("sct")] =
        cbor::Value("SCT", cbor::Value::Type::BYTE_STRING);
    cbor_map[cbor::Value("cert")] =
        cbor::Value(cert_data, cbor::Value::Type::BYTE_STRING);
    cbor_map[cbor::Value("ocsp")] =
        cbor::Value("OCSP", cbor::Value::Type::BYTE_STRING);

    cbor::Value::ArrayValue cbor_array;
    cbor_array.push_back(cbor::Value(u8"\U0001F4DC\u26D3"));
    cbor_array.push_back(cbor::Value(std::move(cbor_map)));

    base::Optional<std::vector<uint8_t>> serialized =
        cbor::Writer::Write(cbor::Value(std::move(cbor_array)));
    if (!serialized)
      return std::string();
    return std::string(reinterpret_cast<char*>(serialized->data()),
                       serialized->size());
  }

  static base::StringPiece CreateCertMessageFromCert(
      const net::X509Certificate& cert) {
    return net::x509_util::CryptoBufferAsStringPiece(cert.cert_buffer());
  }

  static mojo::ScopedDataPipeConsumerHandle CreateTestDataFilledDataPipe() {
    scoped_refptr<net::X509Certificate> certificate = ImportTestCert();
    const std::string message =
        CreateCertMessage(CreateCertMessageFromCert(*certificate));

    mojo::DataPipe data_pipe(message.size());
    CHECK(mojo::BlockingCopyFromString(message, data_pipe.producer_handle));
    return std::move(data_pipe.consumer_handle);
  }

  static net::SHA256HashValue GetTestDataCertFingerprint256() {
    return ImportTestCert()->CalculateChainFingerprint256();
  }

  void RunUntilIdle() { scoped_task_environment_.RunUntilIdle(); }

  std::unique_ptr<SignedExchangeCertFetcher> CreateFetcherAndStart(
      const GURL& url,
      bool force_fetch) {
    SignedExchangeCertFetcher::CertificateCallback callback = base::BindOnce(
        &ForwardCertificateCallback, base::Unretained(&callback_called_),
        base::Unretained(&result_), base::Unretained(&cert_result_));

    return SignedExchangeCertFetcher::CreateAndStart(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &mock_loader_factory_),
        std::move(throttles_), url, request_initiator_, force_fetch,
        SignedExchangeVersion::kB2, std::move(callback),
        nullptr /* devtools_proxy */,
        base::nullopt /* throttling_profile_id */);
  }

  void CallOnReceiveResponse() {
    network::ResourceResponseHead resource_response;
    resource_response.headers =
        base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
    resource_response.headers->AddHeader(
        "Content-Type: application/cert-chain+cbor");
    resource_response.mime_type = "application/cert-chain+cbor";
    mock_loader_factory_.client_ptr()->OnReceiveResponse(resource_response);
  }

  DeferringURLLoaderThrottle* InitializeDeferringURLLoaderThrottle() {
    std::unique_ptr<DeferringURLLoaderThrottle> throttle =
        std::make_unique<DeferringURLLoaderThrottle>();
    DeferringURLLoaderThrottle* ptr = throttle.get();
    throttles_.push_back(std::move(throttle));
    return ptr;
  }

  void CloseClientPipe() { mock_loader_factory_.CloseClientPipe(); }

  const GURL url_;
  const url::Origin request_initiator_;
  bool callback_called_ = false;
  SignedExchangeLoadResult result_;
  std::unique_ptr<SignedExchangeCertificateChain> cert_result_;
  URLLoaderFactoryForMockLoader mock_loader_factory_;
  std::vector<std::unique_ptr<URLLoaderThrottle>> throttles_;

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  ResourceDispatcherHostImpl resource_dispatcher_host_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SignedExchangeCertFetcherTest);
};

}  // namespace

TEST_F(SignedExchangeCertFetcherTest, Simple) {
  std::unique_ptr<SignedExchangeCertFetcher> fetcher =
      CreateFetcherAndStart(url_, false /* force_fetch */);

  ASSERT_TRUE(mock_loader_factory_.client_ptr());
  ASSERT_TRUE(mock_loader_factory_.url_request());
  EXPECT_EQ(url_, mock_loader_factory_.url_request()->url);
  EXPECT_EQ(RESOURCE_TYPE_SUB_RESOURCE,
            mock_loader_factory_.url_request()->resource_type);
  EXPECT_EQ(net::LOAD_DO_NOT_SEND_AUTH_DATA | net::LOAD_DO_NOT_SAVE_COOKIES |
                net::LOAD_DO_NOT_SEND_COOKIES,
            mock_loader_factory_.url_request()->load_flags);
  EXPECT_EQ(request_initiator_,
            mock_loader_factory_.url_request()->request_initiator);

  CallOnReceiveResponse();
  mock_loader_factory_.client_ptr()->OnStartLoadingResponseBody(
      CreateTestDataFilledDataPipe());
  mock_loader_factory_.client_ptr()->OnComplete(
      network::URLLoaderCompletionStatus(net::OK));
  RunUntilIdle();
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(SignedExchangeLoadResult::kSuccess, result_);
  ASSERT_TRUE(cert_result_);
  EXPECT_EQ(GetTestDataCertFingerprint256(),
            cert_result_->cert()->CalculateChainFingerprint256());
}

TEST_F(SignedExchangeCertFetcherTest, MultipleChunked) {
  std::unique_ptr<SignedExchangeCertFetcher> fetcher =
      CreateFetcherAndStart(url_, false /* force_fetch */);
  CallOnReceiveResponse();
  scoped_refptr<net::X509Certificate> certificate = ImportTestCert();
  const std::string message =
      CreateCertMessage(CreateCertMessageFromCert(*certificate));
  mojo::DataPipe data_pipe(message.size() / 2 + 1);
  ASSERT_TRUE(mojo::BlockingCopyFromString(
      message.substr(0, message.size() / 2), data_pipe.producer_handle));
  mock_loader_factory_.client_ptr()->OnStartLoadingResponseBody(
      std::move(data_pipe.consumer_handle));
  RunUntilIdle();
  ASSERT_TRUE(mojo::BlockingCopyFromString(message.substr(message.size() / 2),
                                           data_pipe.producer_handle));
  data_pipe.producer_handle.reset();
  mock_loader_factory_.client_ptr()->OnComplete(
      network::URLLoaderCompletionStatus(net::OK));
  RunUntilIdle();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(SignedExchangeLoadResult::kSuccess, result_);
  ASSERT_TRUE(cert_result_);
  EXPECT_EQ(certificate->CalculateChainFingerprint256(),
            cert_result_->cert()->CalculateChainFingerprint256());
}

TEST_F(SignedExchangeCertFetcherTest, ForceFetchAndFail) {
  std::unique_ptr<SignedExchangeCertFetcher> fetcher =
      CreateFetcherAndStart(url_, true /* force_fetch */);
  CallOnReceiveResponse();

  ASSERT_TRUE(mock_loader_factory_.url_request());
  EXPECT_EQ(url_, mock_loader_factory_.url_request()->url);
  EXPECT_EQ(RESOURCE_TYPE_SUB_RESOURCE,
            mock_loader_factory_.url_request()->resource_type);
  EXPECT_EQ(net::LOAD_DO_NOT_SEND_AUTH_DATA | net::LOAD_DO_NOT_SAVE_COOKIES |
                net::LOAD_DO_NOT_SEND_COOKIES | net::LOAD_DISABLE_CACHE |
                net::LOAD_BYPASS_CACHE,
            mock_loader_factory_.url_request()->load_flags);

  mock_loader_factory_.client_ptr()->OnComplete(
      network::URLLoaderCompletionStatus(net::ERR_INVALID_SIGNED_EXCHANGE));
  RunUntilIdle();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(SignedExchangeLoadResult::kCertFetchError, result_);
  EXPECT_FALSE(cert_result_);
}

TEST_F(SignedExchangeCertFetcherTest, MaxCertSize_Exceeds) {
  scoped_refptr<net::X509Certificate> certificate = ImportTestCert();
  const std::string message =
      CreateCertMessage(CreateCertMessageFromCert(*certificate));
  base::ScopedClosureRunner reset_max =
      SignedExchangeCertFetcher::SetMaxCertSizeForTest(message.size() - 1);

  std::unique_ptr<SignedExchangeCertFetcher> fetcher =
      CreateFetcherAndStart(url_, false /* force_fetch */);
  CallOnReceiveResponse();
  mojo::DataPipe data_pipe(message.size());
  CHECK(mojo::BlockingCopyFromString(message, data_pipe.producer_handle));
  data_pipe.producer_handle.reset();
  mock_loader_factory_.client_ptr()->OnStartLoadingResponseBody(
      std::move(data_pipe.consumer_handle));
  mock_loader_factory_.client_ptr()->OnComplete(
      network::URLLoaderCompletionStatus(net::OK));
  RunUntilIdle();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(SignedExchangeLoadResult::kCertFetchError, result_);
  EXPECT_FALSE(cert_result_);
}

TEST_F(SignedExchangeCertFetcherTest, MaxCertSize_SameSize) {
  scoped_refptr<net::X509Certificate> certificate = ImportTestCert();
  const std::string message =
      CreateCertMessage(CreateCertMessageFromCert(*certificate));
  base::ScopedClosureRunner reset_max =
      SignedExchangeCertFetcher::SetMaxCertSizeForTest(message.size());

  std::unique_ptr<SignedExchangeCertFetcher> fetcher =
      CreateFetcherAndStart(url_, false /* force_fetch */);
  CallOnReceiveResponse();
  mojo::DataPipe data_pipe(message.size());
  CHECK(mojo::BlockingCopyFromString(message, data_pipe.producer_handle));
  data_pipe.producer_handle.reset();
  mock_loader_factory_.client_ptr()->OnStartLoadingResponseBody(
      std::move(data_pipe.consumer_handle));
  mock_loader_factory_.client_ptr()->OnComplete(
      network::URLLoaderCompletionStatus(net::OK));
  RunUntilIdle();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(SignedExchangeLoadResult::kSuccess, result_);
  EXPECT_TRUE(cert_result_);
}

TEST_F(SignedExchangeCertFetcherTest, MaxCertSize_MultipleChunked) {
  scoped_refptr<net::X509Certificate> certificate = ImportTestCert();
  const std::string message =
      CreateCertMessage(CreateCertMessageFromCert(*certificate));
  base::ScopedClosureRunner reset_max =
      SignedExchangeCertFetcher::SetMaxCertSizeForTest(message.size() - 1);

  std::unique_ptr<SignedExchangeCertFetcher> fetcher =
      CreateFetcherAndStart(url_, false /* force_fetch */);
  CallOnReceiveResponse();
  mojo::DataPipe data_pipe(message.size() / 2 + 1);
  ASSERT_TRUE(mojo::BlockingCopyFromString(
      message.substr(0, message.size() / 2), data_pipe.producer_handle));
  mock_loader_factory_.client_ptr()->OnStartLoadingResponseBody(
      std::move(data_pipe.consumer_handle));
  RunUntilIdle();
  ASSERT_TRUE(mojo::BlockingCopyFromString(message.substr(message.size() / 2),
                                           data_pipe.producer_handle));
  data_pipe.producer_handle.reset();
  mock_loader_factory_.client_ptr()->OnComplete(
      network::URLLoaderCompletionStatus(net::OK));
  RunUntilIdle();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(SignedExchangeLoadResult::kCertFetchError, result_);
  EXPECT_FALSE(cert_result_);
}

TEST_F(SignedExchangeCertFetcherTest, MaxCertSize_ContentLengthCheck) {
  scoped_refptr<net::X509Certificate> certificate = ImportTestCert();
  const std::string message =
      CreateCertMessage(CreateCertMessageFromCert(*certificate));
  base::ScopedClosureRunner reset_max =
      SignedExchangeCertFetcher::SetMaxCertSizeForTest(message.size() - 1);

  std::unique_ptr<SignedExchangeCertFetcher> fetcher =
      CreateFetcherAndStart(url_, false /* force_fetch */);
  network::ResourceResponseHead resource_response;
  resource_response.headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  resource_response.content_length = message.size();
  mock_loader_factory_.client_ptr()->OnReceiveResponse(resource_response);
  mojo::DataPipe data_pipe(message.size());
  CHECK(mojo::BlockingCopyFromString(message, data_pipe.producer_handle));
  data_pipe.producer_handle.reset();
  mock_loader_factory_.client_ptr()->OnStartLoadingResponseBody(
      std::move(data_pipe.consumer_handle));
  mock_loader_factory_.client_ptr()->OnComplete(
      network::URLLoaderCompletionStatus(net::OK));
  RunUntilIdle();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(SignedExchangeLoadResult::kCertFetchError, result_);
  EXPECT_FALSE(cert_result_);
}

TEST_F(SignedExchangeCertFetcherTest, Abort_Redirect) {
  std::unique_ptr<SignedExchangeCertFetcher> fetcher =
      CreateFetcherAndStart(url_, false /* force_fetch */);
  network::ResourceResponseHead response_head;
  net::RedirectInfo redirect_info;
  mock_loader_factory_.client_ptr()->OnReceiveRedirect(redirect_info,
                                                       response_head);
  RunUntilIdle();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(SignedExchangeLoadResult::kCertFetchError, result_);
  EXPECT_FALSE(cert_result_);
}

TEST_F(SignedExchangeCertFetcherTest, Abort_404) {
  std::unique_ptr<SignedExchangeCertFetcher> fetcher =
      CreateFetcherAndStart(url_, false /* force_fetch */);
  network::ResourceResponseHead resource_response;
  resource_response.headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 404 Not Found");
  mock_loader_factory_.client_ptr()->OnReceiveResponse(resource_response);
  RunUntilIdle();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(SignedExchangeLoadResult::kCertFetchError, result_);
  EXPECT_FALSE(cert_result_);
}

TEST_F(SignedExchangeCertFetcherTest, WrongMimeType) {
  std::unique_ptr<SignedExchangeCertFetcher> fetcher =
      CreateFetcherAndStart(url_, false /* force_fetch */);
  network::ResourceResponseHead resource_response;
  resource_response.headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  resource_response.headers->AddHeader(
      "Content-Type: application/octet-stream");
  resource_response.mime_type = "application/octet-stream";
  mock_loader_factory_.client_ptr()->OnReceiveResponse(resource_response);
  RunUntilIdle();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(SignedExchangeLoadResult::kCertFetchError, result_);
  EXPECT_FALSE(cert_result_);
}

TEST_F(SignedExchangeCertFetcherTest, Invalid_CertData) {
  std::unique_ptr<SignedExchangeCertFetcher> fetcher =
      CreateFetcherAndStart(url_, false /* force_fetch */);
  CallOnReceiveResponse();
  const std::string message = CreateCertMessage("Invalid Cert Data");
  mojo::DataPipe data_pipe(message.size());
  CHECK(mojo::BlockingCopyFromString(message, data_pipe.producer_handle));
  data_pipe.producer_handle.reset();
  mock_loader_factory_.client_ptr()->OnStartLoadingResponseBody(
      std::move(data_pipe.consumer_handle));
  mock_loader_factory_.client_ptr()->OnComplete(
      network::URLLoaderCompletionStatus(net::OK));
  RunUntilIdle();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(SignedExchangeLoadResult::kCertParseError, result_);
  EXPECT_FALSE(cert_result_);
}

TEST_F(SignedExchangeCertFetcherTest, Invalid_CertMessage) {
  std::unique_ptr<SignedExchangeCertFetcher> fetcher =
      CreateFetcherAndStart(url_, false /* force_fetch */);
  CallOnReceiveResponse();

  const std::string message = "Invalid cert message";

  mojo::DataPipe data_pipe(message.size());
  CHECK(mojo::BlockingCopyFromString(message, data_pipe.producer_handle));
  data_pipe.producer_handle.reset();
  mock_loader_factory_.client_ptr()->OnStartLoadingResponseBody(
      std::move(data_pipe.consumer_handle));

  mock_loader_factory_.client_ptr()->OnComplete(
      network::URLLoaderCompletionStatus(net::OK));
  RunUntilIdle();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(SignedExchangeLoadResult::kCertParseError, result_);
  EXPECT_FALSE(cert_result_);
}

TEST_F(SignedExchangeCertFetcherTest, Throttle_Simple) {
  DeferringURLLoaderThrottle* throttle = InitializeDeferringURLLoaderThrottle();
  std::unique_ptr<SignedExchangeCertFetcher> fetcher =
      CreateFetcherAndStart(url_, false /* force_fetch */);
  RunUntilIdle();

  EXPECT_TRUE(throttle->will_start_request_called());
  EXPECT_FALSE(mock_loader_factory_.url_request());
  EXPECT_FALSE(mock_loader_factory_.client_ptr());

  throttle->delegate()->Resume();

  RunUntilIdle();

  CallOnReceiveResponse();

  RunUntilIdle();

  EXPECT_TRUE(throttle->will_process_response_called());
  mock_loader_factory_.client_ptr()->OnStartLoadingResponseBody(
      CreateTestDataFilledDataPipe());

  mock_loader_factory_.client_ptr()->OnComplete(
      network::URLLoaderCompletionStatus(net::OK));
  RunUntilIdle();

  EXPECT_FALSE(callback_called_);

  throttle->delegate()->Resume();
  RunUntilIdle();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(SignedExchangeLoadResult::kSuccess, result_);
  ASSERT_TRUE(cert_result_);
  EXPECT_EQ(GetTestDataCertFingerprint256(),
            cert_result_->cert()->CalculateChainFingerprint256());
}

TEST_F(SignedExchangeCertFetcherTest, Throttle_AbortsOnRequest) {
  DeferringURLLoaderThrottle* throttle = InitializeDeferringURLLoaderThrottle();
  std::unique_ptr<SignedExchangeCertFetcher> fetcher =
      CreateFetcherAndStart(url_, false /* force_fetch */);
  RunUntilIdle();

  throttle->delegate()->CancelWithError(net::ERR_INVALID_SIGNED_EXCHANGE);

  RunUntilIdle();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(SignedExchangeLoadResult::kCertFetchError, result_);
  EXPECT_FALSE(cert_result_);
}

TEST_F(SignedExchangeCertFetcherTest, Throttle_AbortsOnRedirect) {
  DeferringURLLoaderThrottle* throttle = InitializeDeferringURLLoaderThrottle();
  std::unique_ptr<SignedExchangeCertFetcher> fetcher =
      CreateFetcherAndStart(url_, false /* force_fetch */);
  RunUntilIdle();

  throttle->delegate()->Resume();

  RunUntilIdle();

  network::ResourceResponseHead response_head;
  net::RedirectInfo redirect_info;

  mock_loader_factory_.client_ptr()->OnReceiveRedirect(redirect_info,
                                                       response_head);
  RunUntilIdle();

  EXPECT_TRUE(throttle->will_redirect_request_called());

  throttle->delegate()->CancelWithError(net::ERR_INVALID_SIGNED_EXCHANGE);
  RunUntilIdle();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(SignedExchangeLoadResult::kCertFetchError, result_);
  EXPECT_FALSE(cert_result_);
}

TEST_F(SignedExchangeCertFetcherTest, Throttle_AbortsOnResponse) {
  DeferringURLLoaderThrottle* throttle = InitializeDeferringURLLoaderThrottle();
  std::unique_ptr<SignedExchangeCertFetcher> fetcher =
      CreateFetcherAndStart(url_, false /* force_fetch */);
  RunUntilIdle();

  throttle->delegate()->Resume();

  RunUntilIdle();

  CallOnReceiveResponse();

  RunUntilIdle();

  EXPECT_TRUE(throttle->will_process_response_called());

  mock_loader_factory_.client_ptr()->OnStartLoadingResponseBody(
      CreateTestDataFilledDataPipe());

  mock_loader_factory_.client_ptr()->OnComplete(
      network::URLLoaderCompletionStatus(net::OK));
  RunUntilIdle();

  EXPECT_FALSE(callback_called_);

  throttle->delegate()->CancelWithError(net::ERR_INVALID_SIGNED_EXCHANGE);
  RunUntilIdle();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(SignedExchangeLoadResult::kCertFetchError, result_);
  EXPECT_FALSE(cert_result_);
}

TEST_F(SignedExchangeCertFetcherTest, DeleteFetcher_BeforeReceiveResponse) {
  std::unique_ptr<SignedExchangeCertFetcher> fetcher =
      CreateFetcherAndStart(url_, false /* force_fetch */);
  RunUntilIdle();
  fetcher.reset();
  RunUntilIdle();

  EXPECT_FALSE(callback_called_);
  EXPECT_FALSE(cert_result_);
}

TEST_F(SignedExchangeCertFetcherTest, DeleteFetcher_BeforeResponseBody) {
  std::unique_ptr<SignedExchangeCertFetcher> fetcher =
      CreateFetcherAndStart(url_, false /* force_fetch */);
  CallOnReceiveResponse();
  RunUntilIdle();
  fetcher.reset();
  RunUntilIdle();

  EXPECT_FALSE(callback_called_);
  EXPECT_FALSE(cert_result_);
}

TEST_F(SignedExchangeCertFetcherTest, DeleteFetcher_WhileReceivingBody) {
  std::unique_ptr<SignedExchangeCertFetcher> fetcher =
      CreateFetcherAndStart(url_, false /* force_fetch */);
  CallOnReceiveResponse();
  scoped_refptr<net::X509Certificate> certificate = ImportTestCert();
  const std::string message =
      CreateCertMessage(CreateCertMessageFromCert(*certificate));
  mojo::DataPipe data_pipe(message.size() / 2 + 1);
  ASSERT_TRUE(mojo::BlockingCopyFromString(
      message.substr(0, message.size() / 2), data_pipe.producer_handle));
  mock_loader_factory_.client_ptr()->OnStartLoadingResponseBody(
      std::move(data_pipe.consumer_handle));
  RunUntilIdle();
  fetcher.reset();
  RunUntilIdle();
  ASSERT_TRUE(mojo::BlockingCopyFromString(message.substr(message.size() / 2),
                                           data_pipe.producer_handle));
  RunUntilIdle();

  EXPECT_FALSE(callback_called_);
  EXPECT_FALSE(cert_result_);
}

TEST_F(SignedExchangeCertFetcherTest, DeleteFetcher_AfterReceivingBody) {
  std::unique_ptr<SignedExchangeCertFetcher> fetcher =
      CreateFetcherAndStart(url_, false /* force_fetch */);
  CallOnReceiveResponse();
  scoped_refptr<net::X509Certificate> certificate = ImportTestCert();
  const std::string message =
      CreateCertMessage(CreateCertMessageFromCert(*certificate));
  mojo::DataPipe data_pipe(message.size());
  CHECK(mojo::BlockingCopyFromString(message, data_pipe.producer_handle));
  mock_loader_factory_.client_ptr()->OnStartLoadingResponseBody(
      std::move(data_pipe.consumer_handle));
  RunUntilIdle();
  CloseClientPipe();
  RunUntilIdle();
  data_pipe.producer_handle.reset();
  fetcher.reset();
  RunUntilIdle();

  EXPECT_FALSE(callback_called_);
  EXPECT_FALSE(cert_result_);
}

TEST_F(SignedExchangeCertFetcherTest, CloseClientPipe_BeforeReceiveResponse) {
  std::unique_ptr<SignedExchangeCertFetcher> fetcher =
      CreateFetcherAndStart(url_, false /* force_fetch */);
  RunUntilIdle();
  CloseClientPipe();
  RunUntilIdle();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(SignedExchangeLoadResult::kCertFetchError, result_);
  EXPECT_FALSE(cert_result_);
}

TEST_F(SignedExchangeCertFetcherTest, CloseClientPipe_BeforeResponseBody) {
  std::unique_ptr<SignedExchangeCertFetcher> fetcher =
      CreateFetcherAndStart(url_, false /* force_fetch */);
  CallOnReceiveResponse();
  RunUntilIdle();
  CloseClientPipe();
  RunUntilIdle();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(SignedExchangeLoadResult::kCertFetchError, result_);
  EXPECT_FALSE(cert_result_);
}

TEST_F(SignedExchangeCertFetcherTest, CloseClientPipe_WhileReceivingBody) {
  std::unique_ptr<SignedExchangeCertFetcher> fetcher =
      CreateFetcherAndStart(url_, false /* force_fetch */);
  CallOnReceiveResponse();
  scoped_refptr<net::X509Certificate> certificate = ImportTestCert();
  const std::string message =
      CreateCertMessage(CreateCertMessageFromCert(*certificate));
  mojo::DataPipe data_pipe(message.size() / 2 + 1);
  ASSERT_TRUE(mojo::BlockingCopyFromString(
      message.substr(0, message.size() / 2), data_pipe.producer_handle));
  mock_loader_factory_.client_ptr()->OnStartLoadingResponseBody(
      std::move(data_pipe.consumer_handle));
  RunUntilIdle();
  CloseClientPipe();
  RunUntilIdle();
  data_pipe.producer_handle.reset();
  RunUntilIdle();
  EXPECT_TRUE(callback_called_);
  // SignedExchangeCertFetcher receives a truncated cert cbor.
  EXPECT_EQ(SignedExchangeLoadResult::kCertParseError, result_);
  EXPECT_FALSE(cert_result_);
}

TEST_F(SignedExchangeCertFetcherTest, CloseClientPipe_AfterReceivingBody) {
  std::unique_ptr<SignedExchangeCertFetcher> fetcher =
      CreateFetcherAndStart(url_, false /* force_fetch */);
  CallOnReceiveResponse();
  scoped_refptr<net::X509Certificate> certificate = ImportTestCert();
  const std::string message =
      CreateCertMessage(CreateCertMessageFromCert(*certificate));
  mojo::DataPipe data_pipe(message.size());
  CHECK(mojo::BlockingCopyFromString(message, data_pipe.producer_handle));
  mock_loader_factory_.client_ptr()->OnStartLoadingResponseBody(
      std::move(data_pipe.consumer_handle));
  RunUntilIdle();
  CloseClientPipe();
  RunUntilIdle();
  data_pipe.producer_handle.reset();
  RunUntilIdle();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(SignedExchangeLoadResult::kSuccess, result_);
  ASSERT_TRUE(cert_result_);
  EXPECT_EQ(certificate->CalculateChainFingerprint256(),
            cert_result_->cert()->CalculateChainFingerprint256());
}

TEST_F(SignedExchangeCertFetcherTest, DataURL) {
  const GURL data_url = GURL("data:application/cert-chain+cbor,foobar");
  std::unique_ptr<SignedExchangeCertFetcher> fetcher =
      CreateFetcherAndStart(data_url, false /* force_fetch */);
  EXPECT_EQ(data_url, mock_loader_factory_.url_request()->url);

  network::ResourceResponseHead resource_response;
  resource_response.mime_type = "application/cert-chain+cbor";
  mock_loader_factory_.client_ptr()->OnReceiveResponse(resource_response);

  mock_loader_factory_.client_ptr()->OnStartLoadingResponseBody(
      CreateTestDataFilledDataPipe());
  mock_loader_factory_.client_ptr()->OnComplete(
      network::URLLoaderCompletionStatus(net::OK));
  RunUntilIdle();
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(SignedExchangeLoadResult::kSuccess, result_);
  ASSERT_TRUE(cert_result_);
  EXPECT_EQ(GetTestDataCertFingerprint256(),
            cert_result_->cert()->CalculateChainFingerprint256());
}

TEST_F(SignedExchangeCertFetcherTest, DataURLWithWrongMimeType) {
  const GURL data_url = GURL("data:application/octet-stream,foobar");
  std::unique_ptr<SignedExchangeCertFetcher> fetcher =
      CreateFetcherAndStart(data_url, false /* force_fetch */);
  EXPECT_EQ(data_url, mock_loader_factory_.url_request()->url);

  network::ResourceResponseHead resource_response;
  resource_response.mime_type = "application/octet-stream";
  mock_loader_factory_.client_ptr()->OnReceiveResponse(resource_response);
  RunUntilIdle();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(SignedExchangeLoadResult::kCertFetchError, result_);
}

}  // namespace content
