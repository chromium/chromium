// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_cert_fetcher.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "base/test/task_environment.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "content/public/common/resource_type.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "net/base/load_flags.h"
#include "net/cert/x509_util.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

namespace content {

namespace {

class DeferringURLLoaderThrottle final : public blink::URLLoaderThrottle {
 public:
  DeferringURLLoaderThrottle() = default;
  ~DeferringURLLoaderThrottle() override = default;

  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override {
    will_start_request_called_ = true;
    *defer = true;
  }

  void WillRedirectRequest(
      net::RedirectInfo* redirect_info,
      const network::mojom::URLResponseHead& /* response_head */,
      bool* defer,
      std::vector<std::string>* /* to_be_removed_headers */,
      net::HttpRequestHeaders* /* modified_headers */) override {
    will_redirect_request_called_ = true;
    *defer = true;
  }

  void WillProcessResponse(const GURL& response_url_,
                           network::mojom::URLResponseHead* response_head,
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
  MockURLLoader(
      mojo::PendingReceiver<network::mojom::URLLoader> url_loader_receiver)
      : receiver_(this, std::move(url_loader_receiver)) {}
  ~MockURLLoader() override = default;

  MOCK_METHOD3(FollowRedirect,
               void(const std::vector<std::string>&,
                    const net::HttpRequestHeaders&,
                    const base::Optional<GURL>&));
  MOCK_METHOD2(SetPriority,
               void(net::RequestPriority priority,
                    int32_t intra_priority_value));
  MOCK_METHOD0(PauseReadingBodyFromNet, void());
  MOCK_METHOD0(ResumeReadingBodyFromNet, void());

 private:
  mojo::Receiver<network::mojom::URLLoader> receiver_;

  DISALLOW_COPY_AND_ASSIGN(MockURLLoader);
};

class URLLoaderFactoryForMockLoader final
    : public network::mojom::URLLoaderFactory {
 public:
  URLLoaderFactoryForMockLoader() = default;
  ~URLLoaderFactoryForMockLoader() override = default;

  // network::mojom::URLLoaderFactory implementation.
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> url_loader_receiver,
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& url_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override {
    loader_ = std::make_unique<MockURLLoader>(std::move(url_loader_receiver));
    url_request_ = url_request;
    client_remote_.Bind(std::move(client));
  }

  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory)
      override {
    NOTREACHED();
  }

  mojo::Remote<network::mojom::URLLoaderClient>& client_remote() {
    return client_remote_;
  }
  void CloseClientPipe() { client_remote_.reset(); }

  base::Optional<network::ResourceRequest> url_request() const {
    return url_request_;
  }

 private:
  std::unique_ptr<MockURLLoader> loader_;
  mojo::Remote<network::mojom::URLLoaderClient> client_remote_;
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
      : url_(GURL("https://www.example.com/cert")) {}
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

  static std::string CreateTestData() {
    scoped_refptr<net::X509Certificate> certificate = ImportTestCert();
    return CreateCertMessage(CreateCertMessageFromCert(*certificate));
  }

  static mojo::ScopedDataPipeConsumerHandle CreateTestDataFilledDataPipe() {
    auto message = CreateTestData();
    mojo::DataPipe data_pipe(message.size());
    CHECK(mojo::BlockingCopyFromString(message, data_pipe.producer_handle));
    return std::move(data_pipe.consumer_handle);
  }

  static net::SHA256HashValue GetTestDataCertFingerprint256() {
    return ImportTestCert()->CalculateChainFingerprint256();
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  std::unique_ptr<SignedExchangeCertFetcher> CreateFetcherAndStart(
      const GURL& url,
      bool force_fetch) {
    SignedExchangeCertFetcher::CertificateCallback callback = base::BindOnce(
        &ForwardCertificateCallback, base::Unretained(&callback_called_),
        base::Unretained(&result_), base::Unretained(&cert_result_));

    return SignedExchangeCertFetcher::CreateAndStart(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &mock_loader_factory_),
        std::move(throttles_), url, force_fetch, std::move(callback),
        nullptr /* devtools_proxy */, nullptr /* reporter */,
        base::nullopt /* throttling_profile_id */);
  }

  void CallOnReceiveResponse() {
    auto response_head = network::mojom::URLResponseHead::New();
    response_head->headers =
        base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
    response_head->headers->AddHeader(
        "Content-Type: application/cert-chain+cbor");
    response_head->mime_type = "application/cert-chain+cbor";
    mock_loader_factory_.client_remote()->OnReceiveResponse(
        std::move(response_head));
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
  bool callback_called_ = false;
  SignedExchangeLoadResult result_;
  std::unique_ptr<SignedExchangeCertificateChain> cert_result_;
  URLLoaderFactoryForMockLoader mock_loader_factory_;
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles_;

  base::test::TaskEnvironment task_environment_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SignedExchangeCertFetcherTest);
};

}  // namespace

TEST_F(SignedExchangeCertFetcherTest, Simple) {
  std::unique_ptr<SignedExchangeCertFetcher> fetcher =
      CreateFetcherAndStart(url_, false /* force_fetch */);

  ASSERT_TRUE(mock_loader_factory_.client_remote());
  ASSERT_TRUE(mock_loader_factory_.url_request());
  EXPECT_EQ(url_, mock_loader_factory_.url_request()->url);
  EXPECT_EQ(static_cast<int>(ResourceType::kSubResource),
            mock_loader_factory_.url_request()->resource_type);
  EXPECT_EQ(mock_loader_factory_.url_request()->credentials_mode,
            network::mojom::CredentialsMode::kOmit);
  EXPECT_TRUE(mock_loader_factory_.url_request()->request_initiator->opaque());
  std::string accept;
  EXPECT_TRUE(
      mock_loader_factory_.url_request()->headers.GetHeader("Accept", &accept));
  EXPECT_EQ("application/cert-chain+cbor", accept);

  CallOnReceiveResponse();
  mock_loader_factory_.client_remote()->OnStartLoadingResponseBody(
      CreateTestDataFilledDataPipe());
  mock_loader_factory_.client_remote()->OnComplete(
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
  mock_loader_factory_.client_remote()->OnStartLoadingResponseBody(
      std::move(data_pipe.consumer_handle));
  RunUntilIdle();
  ASSERT_TRUE(mojo::BlockingCopyFromString(message.substr(message.size() / 2),
                                           data_pipe.producer_handle));
  data_pipe.producer_handle.reset();
  mock_loader_factory_.client_remote()->OnComplete(
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
  EXPECT_EQ(static_cast<int>(ResourceType::kSubResource),
            mock_loader_factory_.url_request()->resource_type);
  EXPECT_EQ(net::LOAD_DISABLE_CACHE | net::LOAD_BYPASS_CACHE,
            mock_loader_factory_.url_request()->load_flags);
  EXPECT_EQ(mock_loader_factory_.url_request()->credentials_mode,
            network::mojom::CredentialsMode::kOmit);

  mock_loader_factory_.client_remote()->OnComplete(
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
  mock_loader_factory_.client_remote()->OnStartLoadingResponseBody(
      std::move(data_pipe.consumer_handle));
  mock_loader_factory_.client_remote()->OnComplete(
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
  mock_loader_factory_.client_remote()->OnStartLoadingResponseBody(
      std::move(data_pipe.consumer_handle));
  mock_loader_factory_.client_remote()->OnComplete(
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
  mock_loader_factory_.client_remote()->OnStartLoadingResponseBody(
      std::move(data_pipe.consumer_handle));
  RunUntilIdle();
  ASSERT_TRUE(mojo::BlockingCopyFromString(message.substr(message.size() / 2),
                                           data_pipe.producer_handle));
  data_pipe.producer_handle.reset();
  mock_loader_factory_.client_remote()->OnComplete(
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
  auto response_head = network::mojom::URLResponseHead::New();
  response_head->headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  response_head->content_length = message.size();
  mock_loader_factory_.client_remote()->OnReceiveResponse(
      std::move(response_head));
  mojo::DataPipe data_pipe(message.size());
  CHECK(mojo::BlockingCopyFromString(message, data_pipe.producer_handle));
  data_pipe.producer_handle.reset();
  mock_loader_factory_.client_remote()->OnStartLoadingResponseBody(
      std::move(data_pipe.consumer_handle));
  mock_loader_factory_.client_remote()->OnComplete(
      network::URLLoaderCompletionStatus(net::OK));
  RunUntilIdle();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(SignedExchangeLoadResult::kCertFetchError, result_);
  EXPECT_FALSE(cert_result_);
}

TEST_F(SignedExchangeCertFetcherTest, Abort_Redirect) {
  std::unique_ptr<SignedExchangeCertFetcher> fetcher =
      CreateFetcherAndStart(url_, false /* force_fetch */);
  net::RedirectInfo redirect_info;
  mock_loader_factory_.client_remote()->OnReceiveRedirect(
      redirect_info, network::mojom::URLResponseHead::New());
  RunUntilIdle();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(SignedExchangeLoadResult::kCertFetchError, result_);
  EXPECT_FALSE(cert_result_);
}

TEST_F(SignedExchangeCertFetcherTest, Abort_404) {
  std::unique_ptr<SignedExchangeCertFetcher> fetcher =
      CreateFetcherAndStart(url_, false /* force_fetch */);
  auto response_head = network::mojom::URLResponseHead::New();
  response_head->headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 404 Not Found");
  mock_loader_factory_.client_remote()->OnReceiveResponse(
      std::move(response_head));
  RunUntilIdle();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(SignedExchangeLoadResult::kCertFetchError, result_);
  EXPECT_FALSE(cert_result_);
}

TEST_F(SignedExchangeCertFetcherTest, WrongMimeType) {
  std::unique_ptr<SignedExchangeCertFetcher> fetcher =
      CreateFetcherAndStart(url_, false /* force_fetch */);
  auto response_head = network::mojom::URLResponseHead::New();
  response_head->headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  response_head->headers->AddHeader("Content-Type: application/octet-stream");
  response_head->mime_type = "application/octet-stream";
  mock_loader_factory_.client_remote()->OnReceiveResponse(
      std::move(response_head));
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
  mock_loader_factory_.client_remote()->OnStartLoadingResponseBody(
      std::move(data_pipe.consumer_handle));
  mock_loader_factory_.client_remote()->OnComplete(
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
  mock_loader_factory_.client_remote()->OnStartLoadingResponseBody(
      std::move(data_pipe.consumer_handle));

  mock_loader_factory_.client_remote()->OnComplete(
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
  EXPECT_FALSE(mock_loader_factory_.client_remote());

  throttle->delegate()->Resume();

  RunUntilIdle();

  CallOnReceiveResponse();

  RunUntilIdle();

  EXPECT_TRUE(throttle->will_process_response_called());
  mock_loader_factory_.client_remote()->OnStartLoadingResponseBody(
      CreateTestDataFilledDataPipe());

  mock_loader_factory_.client_remote()->OnComplete(
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

  net::RedirectInfo redirect_info;

  mock_loader_factory_.client_remote()->OnReceiveRedirect(
      redirect_info, network::mojom::URLResponseHead::New());
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

  mock_loader_factory_.client_remote()->OnStartLoadingResponseBody(
      CreateTestDataFilledDataPipe());

  mock_loader_factory_.client_remote()->OnComplete(
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
  mock_loader_factory_.client_remote()->OnStartLoadingResponseBody(
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
  mock_loader_factory_.client_remote()->OnStartLoadingResponseBody(
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
  mock_loader_factory_.client_remote()->OnStartLoadingResponseBody(
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
  mock_loader_factory_.client_remote()->OnStartLoadingResponseBody(
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
  std::string data_url_string = "data:application/cert-chain+cbor";
  std::string output;
  base::Base64Encode(CreateTestData(), &output);
  data_url_string += ";base64," + output;
  const GURL data_url = GURL(data_url_string);
  std::unique_ptr<SignedExchangeCertFetcher> fetcher =
      CreateFetcherAndStart(data_url, false /* force_fetch */);

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

  RunUntilIdle();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(SignedExchangeLoadResult::kCertFetchError, result_);
}

}  // namespace content
