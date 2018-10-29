// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_handler.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/web_package/signed_exchange_cert_fetcher_factory.h"
#include "content/browser/web_package/signed_exchange_devtools_proxy.h"
#include "content/browser/web_package/signed_exchange_signature_verifier.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/ct_policy_enforcer.h"
#include "net/cert/ct_verifier.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/filter/mock_source_stream.h"
#include "net/http/transport_security_state.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/network_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::ElementsAre;
using testing::Property;
using testing::Return;
using testing::SetArgPointee;
using testing::Truly;

namespace content {

namespace {

const uint64_t kSignatureHeaderDate = 1520834000;
const int kOutputBufferSize = 4096;

constexpr char kTestSxgInnerURL[] = "https://test.example.org/test/";

// "wildcard_example.org.public.pem.cbor" has these dummy data in "ocsp" and
// "sct" fields.
constexpr base::StringPiece kDummyOCSPDer = "OCSP";
constexpr char kDummySCTBytes[] = {
    0x00, 0x05,                // Length of the sct list
    0x00, 0x03, 'S', 'C', 'T'  // List entry: length and body
};
constexpr base::StringPiece kDummySCTList(kDummySCTBytes,
                                          sizeof(kDummySCTBytes));

std::string GetTestFileContents(base::StringPiece name) {
  base::FilePath path;
  base::PathService::Get(content::DIR_TEST_DATA, &path);
  path = path.AppendASCII("sxg").AppendASCII(name);

  std::string contents;
  CHECK(base::ReadFileToString(path, &contents));
  return contents;
}

scoped_refptr<net::X509Certificate> LoadCertificate(
    const std::string& cert_file) {
  base::FilePath dir_path;
  base::PathService::Get(content::DIR_TEST_DATA, &dir_path);
  dir_path = dir_path.AppendASCII("sxg");

  base::ScopedAllowBlockingForTesting allow_io;
  return net::CreateCertificateChainFromFile(
      dir_path, cert_file, net::X509Certificate::FORMAT_PEM_CERT_SEQUENCE);
}

class MockSignedExchangeCertFetcherFactory
    : public SignedExchangeCertFetcherFactory {
 public:
  void ExpectFetch(const GURL& cert_url, const std::string& cert_str) {
    expected_cert_url_ = cert_url;
    cert_str_ = cert_str;
  }

  std::unique_ptr<SignedExchangeCertFetcher> CreateFetcherAndStart(
      const GURL& cert_url,
      bool force_fetch,
      SignedExchangeVersion version,
      SignedExchangeCertFetcher::CertificateCallback callback,
      SignedExchangeDevToolsProxy* devtools_proxy) override {
    EXPECT_EQ(cert_url, expected_cert_url_);

    auto cert_chain = SignedExchangeCertificateChain::Parse(
        version, base::as_bytes(base::make_span(cert_str_)), devtools_proxy);
    EXPECT_TRUE(cert_chain);

    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), SignedExchangeLoadResult::kSuccess,
                       std::move(cert_chain)));
    return nullptr;
  }

 private:
  GURL expected_cert_url_;
  std::string cert_str_;
};

class GMockCertVerifier : public net::CertVerifier {
 public:
  // net::CompletionOnceCallback is move-only, which GMock does not support.
  int Verify(const net::CertVerifier::RequestParams& params,
             net::CertVerifyResult* verify_result,
             net::CompletionOnceCallback callback,
             std::unique_ptr<net::CertVerifier::Request>* out_req,
             const net::NetLogWithSource& net_log) override {
    return VerifyImpl(params, verify_result, out_req, net_log);
  }

  MOCK_METHOD4(VerifyImpl,
               int(const net::CertVerifier::RequestParams& params,
                   net::CertVerifyResult* verify_result,
                   std::unique_ptr<net::CertVerifier::Request>* out_req,
                   const net::NetLogWithSource& net_log));
  MOCK_METHOD1(SetConfig, void(const net::CertVerifier::Config& config));
};

class MockCTVerifier : public net::CTVerifier {
 public:
  MOCK_METHOD6(Verify,
               void(base::StringPiece hostname,
                    net::X509Certificate* cert,
                    base::StringPiece stapled_ocsp_response,
                    base::StringPiece sct_list_from_tls_extension,
                    net::SignedCertificateTimestampAndStatusList* output_scts,
                    const net::NetLogWithSource& net_log));
  MOCK_METHOD1(SetObserver, void(CTVerifier::Observer*));
  MOCK_CONST_METHOD0(GetObserver, CTVerifier::Observer*());
};

class MockCTPolicyEnforcer : public net::CTPolicyEnforcer {
 public:
  MOCK_METHOD3(
      CheckCompliance,
      net::ct::CTPolicyCompliance(net::X509Certificate* cert,
                                  const net::ct::SCTList& verified_scts,
                                  const net::NetLogWithSource& net_log));
};

// Matcher to compare two net::X509Certificates
MATCHER_P(CertEqualsIncludingChain, cert, "") {
  return arg->EqualsIncludingChain(cert.get());
}

}  // namespace

class SignedExchangeHandlerTest
    : public ::testing::TestWithParam<net::MockSourceStream::Mode> {
 public:
  SignedExchangeHandlerTest()
      : request_initiator_(
            url::Origin::Create(GURL("https://sxg.example.com/test.sxg"))) {}

  virtual std::string ContentType() {
    return "application/signed-exchange;v=b2";
  }

  void SetUp() override {
    SignedExchangeHandler::SetVerificationTimeForTesting(
        base::Time::UnixEpoch() +
        base::TimeDelta::FromSeconds(kSignatureHeaderDate));
    feature_list_.InitAndEnableFeature(features::kSignedHTTPExchange);

    source_stream_ = std::make_unique<net::MockSourceStream>();
    source_stream_->set_read_one_byte_at_a_time(true);
    source_ = source_stream_.get();
    cert_fetcher_factory_ =
        std::make_unique<MockSignedExchangeCertFetcherFactory>();
    mock_cert_fetcher_factory_ = cert_fetcher_factory_.get();
    mock_ct_policy_enforcer_ = std::make_unique<MockCTPolicyEnforcer>();

    // Lets mock CT policy enforcer return CT_POLICY_COMPLIES_VIA_SCTS by
    // default. This may be overridden by test cases.
    EXPECT_CALL(*mock_ct_policy_enforcer_, CheckCompliance(_, _, _))
        .WillRepeatedly(
            Return(net::ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS));
  }

  void TearDown() override {
    SignedExchangeHandler::SetNetworkContextForTesting(nullptr);
    network::NetworkContext::SetCertVerifierForTesting(nullptr);
    SignedExchangeHandler::SetVerificationTimeForTesting(
        base::Optional<base::Time>());
  }

  void SetCertVerifier(std::unique_ptr<net::CertVerifier> cert_verifier) {
    cert_verifier_ = std::move(cert_verifier);
    network::NetworkContext::SetCertVerifierForTesting(cert_verifier_.get());
  }

  // Reads from |stream| until an error occurs or the EOF is reached.
  // When an error occurs, returns the net error code. When an EOF is reached,
  // returns the number of bytes read. If |output| is non-null, appends data
  // read to it.
  int ReadStream(net::SourceStream* stream, std::string* output) {
    scoped_refptr<net::IOBuffer> output_buffer =
        base::MakeRefCounted<net::IOBuffer>(kOutputBufferSize);
    int bytes_read = 0;
    while (true) {
      net::TestCompletionCallback callback;
      int rv = stream->Read(output_buffer.get(), kOutputBufferSize,
                            callback.callback());
      if (rv == net::ERR_IO_PENDING) {
        while (source_->awaiting_completion())
          source_->CompleteNextRead();
        rv = callback.WaitForResult();
      }
      if (rv == net::OK)
        break;
      if (rv < net::OK)
        return rv;
      EXPECT_GT(rv, net::OK);
      bytes_read += rv;
      if (output)
        output->append(output_buffer->data(), rv);
    }
    return bytes_read;
  }

  int ReadPayloadStream(std::string* output) {
    return ReadStream(payload_stream_.get(), output);
  }

  bool read_header() const { return read_header_; }
  SignedExchangeLoadResult result() const { return result_; }
  net::Error error() const { return error_; }
  const GURL& inner_url() const { return inner_url_; }
  const network::ResourceResponseHead& resource_response() const {
    return resource_response_;
  }

  // Creates a TestURLRequestContext that uses |mock_ct_policy_enforcer_|.
  std::unique_ptr<net::TestURLRequestContext> CreateTestURLRequestContext() {
    auto test_url_request_context =
        std::make_unique<net::TestURLRequestContext>(
            true /* delay_initialization */);
    test_url_request_context->set_ct_policy_enforcer(
        mock_ct_policy_enforcer_.get());
    test_url_request_context->Init();
    return test_url_request_context;
  }

  void CreateSignedExchangeHandler(
      std::unique_ptr<net::TestURLRequestContext> context) {
    url_request_context_ = std::move(context);
    network_context_ = std::make_unique<network::NetworkContext>(
        nullptr, mojo::MakeRequest(&network_context_ptr_),
        url_request_context_.get());
    SignedExchangeHandler::SetNetworkContextForTesting(network_context_.get());

    handler_ = std::make_unique<SignedExchangeHandler>(
        ContentType(), std::move(source_stream_),
        base::BindOnce(&SignedExchangeHandlerTest::OnHeaderFound,
                       base::Unretained(this)),
        std::move(cert_fetcher_factory_), net::LOAD_NORMAL,
        nullptr /* devtools_proxy */, base::RepeatingCallback<int(void)>());
  }

  void WaitForHeader() {
    while (!read_header()) {
      while (source_->awaiting_completion())
        source_->CompleteNextRead();
      browser_thread_bundle_.RunUntilIdle();
    }
  }

  void ExpectHistogramValues(
      base::Optional<SignedExchangeSignatureVerifier::Result> signature_result,
      base::Optional<int32_t> cert_result,
      base::Optional<net::ct::CTPolicyCompliance> ct_result,
      base::Optional<net::OCSPVerifyResult::ResponseStatus>
          ocsp_response_status,
      base::Optional<net::OCSPRevocationStatus> ocsp_revocation_status) {
    // CertVerificationResult histogram records negated net::Error code.
    if (cert_result.has_value())
      *cert_result = -*cert_result;

    ExpectZeroOrUniqueSample("SignedExchange.SignatureVerificationResult",
                             signature_result);
    ExpectZeroOrUniqueSample("SignedExchange.CertVerificationResult",
                             cert_result);
    ExpectZeroOrUniqueSample("SignedExchange.CTVerificationResult", ct_result);
    ExpectZeroOrUniqueSample("SignedExchange.OCSPResponseStatus",
                             ocsp_response_status);
    ExpectZeroOrUniqueSample("SignedExchange.OCSPRevocationStatus",
                             ocsp_revocation_status);
  }

 protected:
  const base::HistogramTester histogram_tester_;
  MockSignedExchangeCertFetcherFactory* mock_cert_fetcher_factory_;
  std::unique_ptr<net::CertVerifier> cert_verifier_;
  std::unique_ptr<MockCTVerifier> mock_ct_verifier_;
  std::unique_ptr<MockCTPolicyEnforcer> mock_ct_policy_enforcer_;
  net::MockSourceStream* source_;
  std::unique_ptr<SignedExchangeHandler> handler_;

 private:
  void OnHeaderFound(SignedExchangeLoadResult result,
                     net::Error error,
                     const GURL& url,
                     const std::string&,
                     const network::ResourceResponseHead& resource_response,
                     std::unique_ptr<net::SourceStream> payload_stream) {
    read_header_ = true;
    result_ = result;
    error_ = error;
    inner_url_ = url;
    resource_response_ = resource_response;
    payload_stream_ = std::move(payload_stream);
  }

  template <typename T>
  void ExpectZeroOrUniqueSample(const std::string& histogram_name,
                                base::Optional<T> expected_value) {
    if (expected_value.has_value())
      histogram_tester_.ExpectUniqueSample(histogram_name, *expected_value, 1);
    else
      histogram_tester_.ExpectTotalCount(histogram_name, 0);
  }

  base::test::ScopedFeatureList feature_list_;
  content::TestBrowserThreadBundle browser_thread_bundle_;
  std::unique_ptr<net::TestURLRequestContext> url_request_context_;
  std::unique_ptr<network::NetworkContext> network_context_;
  network::mojom::NetworkContextPtr network_context_ptr_;
  const url::Origin request_initiator_;
  std::unique_ptr<net::MockSourceStream> source_stream_;
  std::unique_ptr<MockSignedExchangeCertFetcherFactory> cert_fetcher_factory_;

  bool read_header_ = false;
  SignedExchangeLoadResult result_;
  net::Error error_;
  GURL inner_url_;
  network::ResourceResponseHead resource_response_;
  std::unique_ptr<net::SourceStream> payload_stream_;
};

TEST_P(SignedExchangeHandlerTest, Empty) {
  source_->AddReadResult(nullptr, 0, net::OK, GetParam());

  CreateSignedExchangeHandler(CreateTestURLRequestContext());
  WaitForHeader();

  ASSERT_TRUE(read_header());
  EXPECT_EQ(SignedExchangeLoadResult::kFallbackURLParseError, result());
  EXPECT_EQ(net::ERR_INVALID_SIGNED_EXCHANGE, error());
  EXPECT_TRUE(inner_url().is_empty());
}

TEST_P(SignedExchangeHandlerTest, Simple) {
  mock_cert_fetcher_factory_->ExpectFetch(
      GURL("https://cert.example.org/cert.msg"),
      GetTestFileContents("test.example.org.public.pem.cbor"));

  // Make the MockCertVerifier treat the certificate
  // "prime256v1-sha256.public.pem" as valid for "test.example.org".
  scoped_refptr<net::X509Certificate> original_cert =
      LoadCertificate("prime256v1-sha256.public.pem");
  net::CertVerifyResult dummy_result;
  dummy_result.verified_cert = original_cert;
  dummy_result.cert_status = net::OK;
  dummy_result.ocsp_result.response_status = net::OCSPVerifyResult::PROVIDED;
  dummy_result.ocsp_result.revocation_status = net::OCSPRevocationStatus::GOOD;
  auto mock_cert_verifier = std::make_unique<net::MockCertVerifier>();
  mock_cert_verifier->AddResultForCertAndHost(original_cert, "test.example.org",
                                              dummy_result, net::OK);
  SetCertVerifier(std::move(mock_cert_verifier));

  std::string contents = GetTestFileContents("test.example.org_test.sxg");
  source_->AddReadResult(contents.data(), contents.size(), net::OK, GetParam());
  source_->AddReadResult(nullptr, 0, net::OK, GetParam());

  CreateSignedExchangeHandler(CreateTestURLRequestContext());
  WaitForHeader();

  ASSERT_TRUE(read_header());
  EXPECT_EQ(SignedExchangeLoadResult::kSuccess, result());
  EXPECT_EQ(net::OK, error());
  EXPECT_EQ(kTestSxgInnerURL, inner_url());
  EXPECT_EQ(200, resource_response().headers->response_code());
  EXPECT_EQ("text/html", resource_response().mime_type);
  EXPECT_EQ("utf-8", resource_response().charset);
  EXPECT_FALSE(resource_response().load_timing.request_start_time.is_null());
  EXPECT_FALSE(resource_response().load_timing.request_start.is_null());
  EXPECT_FALSE(resource_response().load_timing.send_start.is_null());
  EXPECT_FALSE(resource_response().load_timing.send_end.is_null());
  EXPECT_FALSE(resource_response().load_timing.receive_headers_end.is_null());

  std::string payload;
  int rv = ReadPayloadStream(&payload);

  std::string expected_payload = GetTestFileContents("test.html");

  EXPECT_EQ(payload, expected_payload);
  EXPECT_EQ(rv, static_cast<int>(expected_payload.size()));
  ExpectHistogramValues(
      SignedExchangeSignatureVerifier::Result::kSuccess, net::OK,
      net::ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS,
      net::OCSPVerifyResult::PROVIDED, net::OCSPRevocationStatus::GOOD);
}

TEST_P(SignedExchangeHandlerTest, MimeType) {
  mock_cert_fetcher_factory_->ExpectFetch(
      GURL("https://cert.example.org/cert.msg"),
      GetTestFileContents("test.example.org.public.pem.cbor"));

  // Make the MockCertVerifier treat the certificate
  // "prime256v1-sha256.public.pem" as valid for "test.example.org".
  scoped_refptr<net::X509Certificate> original_cert =
      LoadCertificate("prime256v1-sha256.public.pem");
  net::CertVerifyResult dummy_result;
  dummy_result.verified_cert = original_cert;
  dummy_result.cert_status = net::OK;
  dummy_result.ocsp_result.response_status = net::OCSPVerifyResult::PROVIDED;
  dummy_result.ocsp_result.revocation_status = net::OCSPRevocationStatus::GOOD;
  auto mock_cert_verifier = std::make_unique<net::MockCertVerifier>();
  mock_cert_verifier->AddResultForCertAndHost(original_cert, "test.example.org",
                                              dummy_result, net::OK);
  SetCertVerifier(std::move(mock_cert_verifier));

  std::string contents = GetTestFileContents("test.example.org_hello.txt.sxg");
  source_->AddReadResult(contents.data(), contents.size(), net::OK, GetParam());
  source_->AddReadResult(nullptr, 0, net::OK, GetParam());

  CreateSignedExchangeHandler(CreateTestURLRequestContext());
  WaitForHeader();

  ASSERT_TRUE(read_header());
  EXPECT_EQ(SignedExchangeLoadResult::kSuccess, result());
  EXPECT_EQ(net::OK, error());
  EXPECT_EQ(200, resource_response().headers->response_code());
  EXPECT_EQ("text/plain", resource_response().mime_type);
  EXPECT_EQ("iso-8859-1", resource_response().charset);

  std::string payload;
  int rv = ReadPayloadStream(&payload);

  std::string expected_payload = GetTestFileContents("hello.txt");

  EXPECT_EQ(payload, expected_payload);
  EXPECT_EQ(rv, static_cast<int>(expected_payload.size()));
}

TEST_P(SignedExchangeHandlerTest, HeaderParseError) {
  const uint8_t data[] = {'s',  'x',  'g',  '1',  '-',  'b',  '2',  '\0',
                          0x00, 0x00, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00};
  source_->AddReadResult(reinterpret_cast<const char*>(data), sizeof(data),
                         net::OK, GetParam());
  CreateSignedExchangeHandler(CreateTestURLRequestContext());
  WaitForHeader();

  ASSERT_TRUE(read_header());
  EXPECT_EQ(SignedExchangeLoadResult::kFallbackURLParseError, result());
  EXPECT_EQ(net::ERR_INVALID_SIGNED_EXCHANGE, error());
  EXPECT_TRUE(inner_url().is_empty());
}

TEST_P(SignedExchangeHandlerTest, TruncatedAfterFallbackUrl) {
  std::string contents = GetTestFileContents("test.example.org_test.sxg");
  contents.resize(50);
  source_->AddReadResult(contents.data(), contents.size(), net::OK, GetParam());
  source_->AddReadResult(nullptr, 0, net::OK, GetParam());

  CreateSignedExchangeHandler(CreateTestURLRequestContext());
  WaitForHeader();

  ASSERT_TRUE(read_header());
  EXPECT_EQ(SignedExchangeLoadResult::kHeaderParseError, result());
  EXPECT_EQ(net::ERR_INVALID_SIGNED_EXCHANGE, error());
  EXPECT_TRUE(inner_url().is_valid());
}

TEST_P(SignedExchangeHandlerTest, CertWithoutExtensionShouldBeRejected) {
  mock_cert_fetcher_factory_->ExpectFetch(
      GURL("https://cert.example.org/cert.msg"),
      GetTestFileContents("test.example.org-noext.public.pem.cbor"));

  // Make the MockCertVerifier treat the certificate
  // "prime256v1-sha256.public.pem" as valid for "test.example.org".
  scoped_refptr<net::X509Certificate> original_cert =
      LoadCertificate("prime256v1-sha256-noext.public.pem");
  net::CertVerifyResult dummy_result;
  dummy_result.verified_cert = original_cert;
  dummy_result.cert_status = net::OK;
  dummy_result.ocsp_result.response_status = net::OCSPVerifyResult::PROVIDED;
  dummy_result.ocsp_result.revocation_status = net::OCSPRevocationStatus::GOOD;
  auto mock_cert_verifier = std::make_unique<net::MockCertVerifier>();
  mock_cert_verifier->AddResultForCertAndHost(original_cert, "test.example.org",
                                              dummy_result, net::OK);
  SetCertVerifier(std::move(mock_cert_verifier));

  std::string contents = GetTestFileContents("test.example.org_noext_test.sxg");
  source_->AddReadResult(contents.data(), contents.size(), net::OK, GetParam());
  source_->AddReadResult(nullptr, 0, net::OK, GetParam());

  CreateSignedExchangeHandler(CreateTestURLRequestContext());
  WaitForHeader();

  ASSERT_TRUE(read_header());
  EXPECT_EQ(SignedExchangeLoadResult::kCertRequirementsNotMet, result());
  EXPECT_EQ(net::ERR_INVALID_SIGNED_EXCHANGE, error());
  EXPECT_EQ(kTestSxgInnerURL, inner_url());
  // Drain the MockSourceStream, otherwise its destructer causes DCHECK failure.
  ReadStream(source_, nullptr);
}

TEST_P(SignedExchangeHandlerTest, CertWithoutExtensionAllowedByFeatureFlag) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(
      features::kAllowSignedHTTPExchangeCertsWithoutExtension);

  mock_cert_fetcher_factory_->ExpectFetch(
      GURL("https://cert.example.org/cert.msg"),
      GetTestFileContents("test.example.org-noext.public.pem.cbor"));

  // Make the MockCertVerifier treat the certificate
  // "prime256v1-sha256.public.pem" as valid for "test.example.org".
  scoped_refptr<net::X509Certificate> original_cert =
      LoadCertificate("prime256v1-sha256-noext.public.pem");
  net::CertVerifyResult dummy_result;
  dummy_result.verified_cert = original_cert;
  dummy_result.cert_status = net::OK;
  dummy_result.ocsp_result.response_status = net::OCSPVerifyResult::PROVIDED;
  dummy_result.ocsp_result.revocation_status = net::OCSPRevocationStatus::GOOD;
  auto mock_cert_verifier = std::make_unique<net::MockCertVerifier>();
  mock_cert_verifier->AddResultForCertAndHost(original_cert, "test.example.org",
                                              dummy_result, net::OK);
  SetCertVerifier(std::move(mock_cert_verifier));

  std::string contents = GetTestFileContents("test.example.org_noext_test.sxg");
  source_->AddReadResult(contents.data(), contents.size(), net::OK, GetParam());
  source_->AddReadResult(nullptr, 0, net::OK, GetParam());

  CreateSignedExchangeHandler(CreateTestURLRequestContext());
  WaitForHeader();

  ASSERT_TRUE(read_header());
  EXPECT_EQ(SignedExchangeLoadResult::kSuccess, result());
  EXPECT_EQ(net::OK, error());
  std::string payload;
  int rv = ReadPayloadStream(&payload);
  std::string expected_payload = GetTestFileContents("test.html");

  EXPECT_EQ(expected_payload, payload);
  EXPECT_EQ(static_cast<int>(expected_payload.size()), rv);
}

TEST_P(SignedExchangeHandlerTest, CertSha256Mismatch) {
  // The certificate is for "127.0.0.1". And the SHA 256 hash of the certificate
  // is different from the cert-sha256 of the signature in the sxg file. So the
  // certification verification must fail.
  mock_cert_fetcher_factory_->ExpectFetch(
      GURL("https://cert.example.org/cert.msg"),
      GetTestFileContents("127.0.0.1.public.pem.cbor"));

  // Set the default result of MockCertVerifier to OK, to check that the
  // verification of SignedExchange must fail even if the certificate is valid.
  auto mock_cert_verifier = std::make_unique<net::MockCertVerifier>();
  mock_cert_verifier->set_default_result(net::OK);
  SetCertVerifier(std::move(mock_cert_verifier));

  std::string contents = GetTestFileContents("test.example.org_test.sxg");
  source_->AddReadResult(contents.data(), contents.size(), net::OK, GetParam());
  source_->AddReadResult(nullptr, 0, net::OK, GetParam());

  CreateSignedExchangeHandler(CreateTestURLRequestContext());
  WaitForHeader();

  ASSERT_TRUE(read_header());
  EXPECT_EQ(SignedExchangeLoadResult::kSignatureVerificationError, result());
  EXPECT_EQ(net::ERR_INVALID_SIGNED_EXCHANGE, error());
  EXPECT_EQ(kTestSxgInnerURL, inner_url());
  ExpectHistogramValues(
      SignedExchangeSignatureVerifier::Result::kErrCertificateSHA256Mismatch,
      base::nullopt /* cert_result */, base::nullopt /* ct_result */,
      base::nullopt /* ocsp_response_status */,
      base::nullopt /* ocsp_revocation_status */);

  // Drain the MockSourceStream, otherwise its destructer causes DCHECK failure.
  ReadStream(source_, nullptr);
}

TEST_P(SignedExchangeHandlerTest, VerifyCertFailure) {
  mock_cert_fetcher_factory_->ExpectFetch(
      GURL("https://cert.example.org/cert.msg"),
      GetTestFileContents("test.example.org.public.pem.cbor"));

  // Make the MockCertVerifier treat the certificate
  // "prime256v1-sha256.public.pem" as valid for "test.example.org".
  scoped_refptr<net::X509Certificate> original_cert =
      LoadCertificate("prime256v1-sha256.public.pem");
  net::CertVerifyResult dummy_result;
  dummy_result.verified_cert = original_cert;
  dummy_result.cert_status = net::OK;
  auto mock_cert_verifier = std::make_unique<net::MockCertVerifier>();
  mock_cert_verifier->AddResultForCertAndHost(original_cert, "test.example.org",
                                              dummy_result, net::OK);
  SetCertVerifier(std::move(mock_cert_verifier));

  // The certificate is for "test.example.org". But the request URL of the sxg
  // file is "https://test.example.com/test/". So the certification verification
  // must fail.
  std::string contents =
      GetTestFileContents("test.example.com_invalid_test.sxg");
  source_->AddReadResult(contents.data(), contents.size(), net::OK, GetParam());
  source_->AddReadResult(nullptr, 0, net::OK, GetParam());

  CreateSignedExchangeHandler(CreateTestURLRequestContext());
  WaitForHeader();

  ASSERT_TRUE(read_header());
  EXPECT_EQ(SignedExchangeLoadResult::kCertVerificationError, result());
  EXPECT_EQ(net::ERR_INVALID_SIGNED_EXCHANGE, error());
  EXPECT_EQ("https://test.example.com/test/", inner_url());
  ExpectHistogramValues(
      SignedExchangeSignatureVerifier::Result::kSuccess, net::ERR_CERT_INVALID,
      net::ct::CTPolicyCompliance::CT_POLICY_COMPLIANCE_DETAILS_NOT_AVAILABLE,
      base::nullopt /* ocsp_response_status */,
      base::nullopt /* ocsp_revocation_status */);

  // Drain the MockSourceStream, otherwise its destructer causes DCHECK failure.
  ReadStream(source_, nullptr);
}

TEST_P(SignedExchangeHandlerTest, OCSPNotChecked) {
  mock_cert_fetcher_factory_->ExpectFetch(
      GURL("https://cert.example.org/cert.msg"),
      GetTestFileContents("test.example.org.public.pem.cbor"));

  // Make the MockCertVerifier treat the certificate
  // "prime256v1-sha256.public.pem" as valid for "test.example.org".
  scoped_refptr<net::X509Certificate> original_cert =
      LoadCertificate("prime256v1-sha256.public.pem");
  net::CertVerifyResult dummy_result;
  dummy_result.verified_cert = original_cert;
  dummy_result.cert_status = net::OK;
  dummy_result.ocsp_result.response_status = net::OCSPVerifyResult::NOT_CHECKED;
  auto mock_cert_verifier = std::make_unique<net::MockCertVerifier>();
  mock_cert_verifier->AddResultForCertAndHost(original_cert, "test.example.org",
                                              dummy_result, net::OK);
  SetCertVerifier(std::move(mock_cert_verifier));

  std::string contents = GetTestFileContents("test.example.org_test.sxg");
  source_->AddReadResult(contents.data(), contents.size(), net::OK, GetParam());
  source_->AddReadResult(nullptr, 0, net::OK, GetParam());

  CreateSignedExchangeHandler(CreateTestURLRequestContext());
  WaitForHeader();

  ASSERT_TRUE(read_header());
  EXPECT_EQ(SignedExchangeLoadResult::kOCSPError, result());
  EXPECT_EQ(net::ERR_INVALID_SIGNED_EXCHANGE, error());
  EXPECT_EQ(kTestSxgInnerURL, inner_url());
  // Drain the MockSourceStream, otherwise its destructer causes DCHECK failure.
  ReadStream(source_, nullptr);
}

TEST_P(SignedExchangeHandlerTest, OCSPNotProvided) {
  mock_cert_fetcher_factory_->ExpectFetch(
      GURL("https://cert.example.org/cert.msg"),
      GetTestFileContents("test.example.org.public.pem.cbor"));

  // Make the MockCertVerifier treat the certificate
  // "prime256v1-sha256.public.pem" as valid for "test.example.org".
  scoped_refptr<net::X509Certificate> original_cert =
      LoadCertificate("prime256v1-sha256.public.pem");
  net::CertVerifyResult dummy_result;
  dummy_result.verified_cert = original_cert;
  dummy_result.cert_status = net::OK;
  dummy_result.ocsp_result.response_status = net::OCSPVerifyResult::MISSING;
  auto mock_cert_verifier = std::make_unique<net::MockCertVerifier>();
  mock_cert_verifier->AddResultForCertAndHost(original_cert, "test.example.org",
                                              dummy_result, net::OK);
  SetCertVerifier(std::move(mock_cert_verifier));

  std::string contents = GetTestFileContents("test.example.org_test.sxg");
  source_->AddReadResult(contents.data(), contents.size(), net::OK, GetParam());
  source_->AddReadResult(nullptr, 0, net::OK, GetParam());

  CreateSignedExchangeHandler(CreateTestURLRequestContext());
  WaitForHeader();

  ASSERT_TRUE(read_header());
  EXPECT_EQ(SignedExchangeLoadResult::kOCSPError, result());
  EXPECT_EQ(net::ERR_INVALID_SIGNED_EXCHANGE, error());
  EXPECT_EQ(kTestSxgInnerURL, inner_url());
  // Drain the MockSourceStream, otherwise its destructer causes DCHECK failure.
  ReadStream(source_, nullptr);
}

TEST_P(SignedExchangeHandlerTest, OCSPInvalid) {
  mock_cert_fetcher_factory_->ExpectFetch(
      GURL("https://cert.example.org/cert.msg"),
      GetTestFileContents("test.example.org.public.pem.cbor"));

  // Make the MockCertVerifier treat the certificate
  // "prime256v1-sha256.public.pem" as valid for "test.example.org".
  scoped_refptr<net::X509Certificate> original_cert =
      LoadCertificate("prime256v1-sha256.public.pem");
  net::CertVerifyResult dummy_result;
  dummy_result.verified_cert = original_cert;
  dummy_result.cert_status = net::OK;
  dummy_result.ocsp_result.response_status =
      net::OCSPVerifyResult::INVALID_DATE;
  auto mock_cert_verifier = std::make_unique<net::MockCertVerifier>();
  mock_cert_verifier->AddResultForCertAndHost(original_cert, "test.example.org",
                                              dummy_result, net::OK);
  SetCertVerifier(std::move(mock_cert_verifier));

  std::string contents = GetTestFileContents("test.example.org_test.sxg");
  source_->AddReadResult(contents.data(), contents.size(), net::OK, GetParam());
  source_->AddReadResult(nullptr, 0, net::OK, GetParam());

  CreateSignedExchangeHandler(CreateTestURLRequestContext());
  WaitForHeader();

  ASSERT_TRUE(read_header());
  EXPECT_EQ(SignedExchangeLoadResult::kOCSPError, result());
  EXPECT_EQ(net::ERR_INVALID_SIGNED_EXCHANGE, error());
  EXPECT_EQ(kTestSxgInnerURL, inner_url());
  // Drain the MockSourceStream, otherwise its destructer causes DCHECK failure.
  ReadStream(source_, nullptr);
}

TEST_P(SignedExchangeHandlerTest, OCSPRevoked) {
  mock_cert_fetcher_factory_->ExpectFetch(
      GURL("https://cert.example.org/cert.msg"),
      GetTestFileContents("test.example.org.public.pem.cbor"));

  // Make the MockCertVerifier treat the certificate
  // "prime256v1-sha256.public.pem" as valid for "test.example.org".
  scoped_refptr<net::X509Certificate> original_cert =
      LoadCertificate("prime256v1-sha256.public.pem");
  net::CertVerifyResult dummy_result;
  dummy_result.verified_cert = original_cert;
  dummy_result.cert_status = net::OK;
  dummy_result.ocsp_result.response_status = net::OCSPVerifyResult::PROVIDED;
  dummy_result.ocsp_result.revocation_status =
      net::OCSPRevocationStatus::REVOKED;
  auto mock_cert_verifier = std::make_unique<net::MockCertVerifier>();
  mock_cert_verifier->AddResultForCertAndHost(original_cert, "test.example.org",
                                              dummy_result, net::OK);
  SetCertVerifier(std::move(mock_cert_verifier));

  std::string contents = GetTestFileContents("test.example.org_test.sxg");
  source_->AddReadResult(contents.data(), contents.size(), net::OK, GetParam());
  source_->AddReadResult(nullptr, 0, net::OK, GetParam());

  CreateSignedExchangeHandler(CreateTestURLRequestContext());
  WaitForHeader();

  ASSERT_TRUE(read_header());
  EXPECT_EQ(SignedExchangeLoadResult::kOCSPError, result());
  EXPECT_EQ(net::ERR_INVALID_SIGNED_EXCHANGE, error());
  EXPECT_EQ(kTestSxgInnerURL, inner_url());
  // Drain the MockSourceStream, otherwise its destructer causes DCHECK failure.
  ReadStream(source_, nullptr);
}

// Test that fetching a signed exchange properly extracts and
// attempts to verify both the certificate and the OCSP response.
TEST_P(SignedExchangeHandlerTest, CertVerifierParams) {
  mock_cert_fetcher_factory_->ExpectFetch(
      GURL("https://cert.example.org/cert.msg"),
      GetTestFileContents("test.example.org.public.pem.cbor"));

  scoped_refptr<net::X509Certificate> original_cert =
      LoadCertificate("prime256v1-sha256.public.pem");
  net::CertVerifyResult fake_result;
  fake_result.verified_cert = original_cert;
  fake_result.cert_status = net::OK;
  fake_result.ocsp_result.response_status = net::OCSPVerifyResult::PROVIDED;
  fake_result.ocsp_result.revocation_status = net::OCSPRevocationStatus::GOOD;

  std::unique_ptr<GMockCertVerifier> gmock_cert_verifier =
      std::make_unique<GMockCertVerifier>();
  EXPECT_CALL(
      *gmock_cert_verifier,
      VerifyImpl(
          AllOf(Property(&net::CertVerifier::RequestParams::ocsp_response,
                         kDummyOCSPDer),
                Property(&net::CertVerifier::RequestParams::certificate,
                         CertEqualsIncludingChain(original_cert)),
                Property(&net::CertVerifier::RequestParams::hostname,
                         "test.example.org")),
          _ /* verify_result */, _ /* out_req */, _ /* net_log */
          ))
      .WillOnce(DoAll(SetArgPointee<1>(fake_result), Return(net::OK)));
  SetCertVerifier(std::move(gmock_cert_verifier));

  std::string contents = GetTestFileContents("test.example.org_test.sxg");
  source_->AddReadResult(contents.data(), contents.size(), net::OK, GetParam());
  source_->AddReadResult(nullptr, 0, net::OK, GetParam());

  CreateSignedExchangeHandler(CreateTestURLRequestContext());
  WaitForHeader();

  ASSERT_TRUE(read_header());
  EXPECT_EQ(SignedExchangeLoadResult::kSuccess, result());
  EXPECT_EQ(net::OK, error());
  std::string payload;
  int rv = ReadPayloadStream(&payload);
  std::string expected_payload = GetTestFileContents("test.html");

  EXPECT_EQ(expected_payload, payload);
  EXPECT_EQ(static_cast<int>(expected_payload.size()), rv);
}

TEST_P(SignedExchangeHandlerTest, NotEnoughSCTsFromPubliclyTrustedCert) {
  mock_cert_fetcher_factory_->ExpectFetch(
      GURL("https://cert.example.org/cert.msg"),
      GetTestFileContents("test.example.org.public.pem.cbor"));

  // Make the MockCertVerifier treat the certificate
  // "prime256v1-sha256.public.pem" as valid for "test.example.org".
  scoped_refptr<net::X509Certificate> original_cert =
      LoadCertificate("prime256v1-sha256.public.pem");
  net::CertVerifyResult dummy_result;
  dummy_result.verified_cert = original_cert;
  dummy_result.is_issued_by_known_root = true;
  dummy_result.cert_status = net::OK;
  dummy_result.ocsp_result.response_status = net::OCSPVerifyResult::PROVIDED;
  dummy_result.ocsp_result.revocation_status = net::OCSPRevocationStatus::GOOD;
  auto mock_cert_verifier = std::make_unique<net::MockCertVerifier>();
  mock_cert_verifier->AddResultForCertAndHost(original_cert, "test.example.org",
                                              dummy_result, net::OK);
  SetCertVerifier(std::move(mock_cert_verifier));

  // Lets the mock CT policy enforcer return CT_POLICY_NOT_ENOUGH_SCTS.
  EXPECT_CALL(*mock_ct_policy_enforcer_, CheckCompliance(_, _, _))
      .WillOnce(Return(net::ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS));

  std::string contents = GetTestFileContents("test.example.org_test.sxg");
  source_->AddReadResult(contents.data(), contents.size(), net::OK, GetParam());
  source_->AddReadResult(nullptr, 0, net::OK, GetParam());

  CreateSignedExchangeHandler(CreateTestURLRequestContext());
  WaitForHeader();

  ASSERT_TRUE(read_header());
  EXPECT_EQ(SignedExchangeLoadResult::kCTVerificationError, result());
  EXPECT_EQ(net::ERR_INVALID_SIGNED_EXCHANGE, error());
  EXPECT_EQ(kTestSxgInnerURL, inner_url());
  ExpectHistogramValues(SignedExchangeSignatureVerifier::Result::kSuccess,
                        net::ERR_CERTIFICATE_TRANSPARENCY_REQUIRED,
                        net::ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS,
                        base::nullopt /* ocsp_response_status */,
                        base::nullopt /* ocsp_revocation_status */);
  // Drain the MockSourceStream, otherwise its destructer causes DCHECK failure.
  ReadStream(source_, nullptr);
}

TEST_P(SignedExchangeHandlerTest, CTRequirementsMetForPubliclyTrustedCert) {
  mock_cert_fetcher_factory_->ExpectFetch(
      GURL("https://cert.example.org/cert.msg"),
      GetTestFileContents("test.example.org.public.pem.cbor"));

  // Make the MockCertVerifier treat the certificate
  // "prime256v1-sha256.public.pem" as valid for "test.example.org".
  scoped_refptr<net::X509Certificate> original_cert =
      LoadCertificate("prime256v1-sha256.public.pem");
  net::CertVerifyResult dummy_result;
  dummy_result.verified_cert = original_cert;
  dummy_result.is_issued_by_known_root = true;
  dummy_result.cert_status = net::CERT_STATUS_IS_EV;
  dummy_result.ocsp_result.response_status = net::OCSPVerifyResult::PROVIDED;
  dummy_result.ocsp_result.revocation_status = net::OCSPRevocationStatus::GOOD;
  auto mock_cert_verifier = std::make_unique<net::MockCertVerifier>();
  mock_cert_verifier->AddResultForCertAndHost(original_cert, "test.example.org",
                                              dummy_result, net::OK);
  SetCertVerifier(std::move(mock_cert_verifier));

  // The mock CT policy enforcer will return CT_POLICY_COMPLIES_VIA_SCTS, as
  // configured in SetUp().

  std::string contents = GetTestFileContents("test.example.org_test.sxg");
  source_->AddReadResult(contents.data(), contents.size(), net::OK, GetParam());
  source_->AddReadResult(nullptr, 0, net::OK, GetParam());

  CreateSignedExchangeHandler(CreateTestURLRequestContext());
  WaitForHeader();

  ASSERT_TRUE(read_header());
  EXPECT_EQ(SignedExchangeLoadResult::kSuccess, result());
  EXPECT_EQ(net::OK, error());
  // EV status should be preserved.
  EXPECT_TRUE(resource_response().ssl_info->cert_status &
              net::CERT_STATUS_IS_EV);
  EXPECT_FALSE(resource_response().ssl_info->cert_status &
               net::CERT_STATUS_CT_COMPLIANCE_FAILED);
  EXPECT_TRUE(resource_response().ssl_info->ct_policy_compliance_required);
  EXPECT_EQ(net::ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS,
            resource_response().ssl_info->ct_policy_compliance);
  ExpectHistogramValues(
      SignedExchangeSignatureVerifier::Result::kSuccess, net::OK,
      net::ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS,
      net::OCSPVerifyResult::PROVIDED, net::OCSPRevocationStatus::GOOD);

  std::string payload;
  int rv = ReadPayloadStream(&payload);
  std::string expected_payload = GetTestFileContents("test.html");
  EXPECT_EQ(expected_payload, payload);
  EXPECT_EQ(static_cast<int>(expected_payload.size()), rv);
}

TEST_P(SignedExchangeHandlerTest, CTNotRequiredForLocalAnchors) {
  mock_cert_fetcher_factory_->ExpectFetch(
      GURL("https://cert.example.org/cert.msg"),
      GetTestFileContents("test.example.org.public.pem.cbor"));

  // Make the MockCertVerifier treat the certificate
  // "prime256v1-sha256.public.pem" as valid for "test.example.org".
  scoped_refptr<net::X509Certificate> original_cert =
      LoadCertificate("prime256v1-sha256.public.pem");
  net::CertVerifyResult dummy_result;
  dummy_result.verified_cert = original_cert;

  // Local anchor.
  dummy_result.is_issued_by_known_root = false;

  dummy_result.cert_status = net::CERT_STATUS_IS_EV;
  dummy_result.ocsp_result.response_status = net::OCSPVerifyResult::PROVIDED;
  dummy_result.ocsp_result.revocation_status = net::OCSPRevocationStatus::GOOD;
  auto mock_cert_verifier = std::make_unique<net::MockCertVerifier>();
  mock_cert_verifier->AddResultForCertAndHost(original_cert, "test.example.org",
                                              dummy_result, net::OK);
  SetCertVerifier(std::move(mock_cert_verifier));
  // Lets the mock CT policy enforcer return CT_POLICY_NOT_ENOUGH_SCTS.
  EXPECT_CALL(*mock_ct_policy_enforcer_, CheckCompliance(_, _, _))
      .WillOnce(Return(net::ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS));

  std::string contents = GetTestFileContents("test.example.org_test.sxg");
  source_->AddReadResult(contents.data(), contents.size(), net::OK, GetParam());
  source_->AddReadResult(nullptr, 0, net::OK, GetParam());

  CreateSignedExchangeHandler(CreateTestURLRequestContext());
  WaitForHeader();

  ASSERT_TRUE(read_header());
  EXPECT_EQ(SignedExchangeLoadResult::kSuccess, result());
  EXPECT_EQ(net::OK, error());
  // EV status should be removed.
  EXPECT_FALSE(resource_response().ssl_info->cert_status &
               net::CERT_STATUS_IS_EV);
  EXPECT_TRUE(resource_response().ssl_info->cert_status &
              net::CERT_STATUS_CT_COMPLIANCE_FAILED);
  EXPECT_FALSE(resource_response().ssl_info->ct_policy_compliance_required);
  EXPECT_EQ(net::ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS,
            resource_response().ssl_info->ct_policy_compliance);
  ExpectHistogramValues(
      SignedExchangeSignatureVerifier::Result::kSuccess, net::OK,
      net::ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS,
      net::OCSPVerifyResult::PROVIDED, net::OCSPRevocationStatus::GOOD);

  std::string payload;
  int rv = ReadPayloadStream(&payload);
  std::string expected_payload = GetTestFileContents("test.html");
  EXPECT_EQ(expected_payload, payload);
  EXPECT_EQ(static_cast<int>(expected_payload.size()), rv);
}

// Test that SignedExchangeHandler calls CTVerifier and CTPolicyEnforcer
// with appropriate arguments.
TEST_P(SignedExchangeHandlerTest, CTVerifierParams) {
  mock_cert_fetcher_factory_->ExpectFetch(
      GURL("https://cert.example.org/cert.msg"),
      GetTestFileContents("test.example.org.public.pem.cbor"));
  scoped_refptr<net::X509Certificate> original_cert =
      LoadCertificate("prime256v1-sha256.public.pem");

  net::SignedCertificateTimestampAndStatusList fake_sct_list;
  auto good_sct = base::MakeRefCounted<net::ct::SignedCertificateTimestamp>();
  fake_sct_list.emplace_back(good_sct, net::ct::SCT_STATUS_OK);
  auto bad_sct = base::MakeRefCounted<net::ct::SignedCertificateTimestamp>();
  fake_sct_list.emplace_back(bad_sct, net::ct::SCT_STATUS_INVALID_TIMESTAMP);

  EXPECT_CALL(*mock_ct_policy_enforcer_,
              CheckCompliance(CertEqualsIncludingChain(original_cert),
                              ElementsAre(good_sct), _ /* net_log */))
      .WillOnce(
          Return(net::ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS));

  mock_ct_verifier_ = std::make_unique<MockCTVerifier>();
  EXPECT_CALL(*mock_ct_verifier_,
              Verify(base::StringPiece("test.example.org"),
                     CertEqualsIncludingChain(original_cert), kDummyOCSPDer,
                     kDummySCTList, _ /* output_scts */, _ /* net_log */))
      .WillOnce(SetArgPointee<4>(fake_sct_list));

  auto test_url_request_context = std::make_unique<net::TestURLRequestContext>(
      true /* delay_initialization */);
  test_url_request_context->set_ct_policy_enforcer(
      mock_ct_policy_enforcer_.get());
  test_url_request_context->set_cert_transparency_verifier(
      mock_ct_verifier_.get());
  test_url_request_context->Init();

  // Make the MockCertVerifier treat the certificate
  // "prime256v1-sha256.public.pem" as valid for "test.example.org".
  net::CertVerifyResult dummy_result;
  dummy_result.verified_cert = original_cert;
  dummy_result.cert_status = net::OK;
  dummy_result.ocsp_result.response_status = net::OCSPVerifyResult::PROVIDED;
  dummy_result.ocsp_result.revocation_status = net::OCSPRevocationStatus::GOOD;
  auto mock_cert_verifier = std::make_unique<net::MockCertVerifier>();
  mock_cert_verifier->AddResultForCertAndHost(original_cert, "test.example.org",
                                              dummy_result, net::OK);
  SetCertVerifier(std::move(mock_cert_verifier));

  std::string contents = GetTestFileContents("test.example.org_test.sxg");
  source_->AddReadResult(contents.data(), contents.size(), net::OK,
                         net::MockSourceStream::ASYNC);
  source_->AddReadResult(nullptr, 0, net::OK, net::MockSourceStream::ASYNC);

  CreateSignedExchangeHandler(std::move(test_url_request_context));
  WaitForHeader();

  ASSERT_TRUE(read_header());
  EXPECT_EQ(SignedExchangeLoadResult::kSuccess, result());
  EXPECT_EQ(net::OK, error());
  std::string payload;
  int rv = ReadPayloadStream(&payload);
  std::string expected_payload = GetTestFileContents("test.html");

  EXPECT_EQ(expected_payload, payload);
  EXPECT_EQ(static_cast<int>(expected_payload.size()), rv);
}

INSTANTIATE_TEST_CASE_P(SignedExchangeHandlerTests,
                        SignedExchangeHandlerTest,
                        ::testing::Values(net::MockSourceStream::SYNC,
                                          net::MockSourceStream::ASYNC));

}  // namespace content
