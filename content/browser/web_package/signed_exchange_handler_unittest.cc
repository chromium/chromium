// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_handler.h"

#include <string_view>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/web_package/signed_exchange_cert_fetcher_factory.h"
#include "content/browser/web_package/signed_exchange_devtools_proxy.h"
#include "content/browser/web_package/signed_exchange_signature_verifier.h"
#include "content/browser/web_package/signed_exchange_test_utils.h"
#include "content/browser/web_package/signed_exchange_utils.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/features.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/ct_policy_enforcer.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/cert/sct_auditing_delegate.h"
#include "net/cert/signed_certificate_timestamp_and_status.h"
#include "net/filter/mock_source_stream.h"
#include "net/http/transport_security_state.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/network_context.h"
#include "services/network/public/cpp/network_service_buildflags.h"
#include "services/network/public/cpp/network_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/web_package/web_package_request_matcher.h"

using testing::_;
using testing::DoAll;
using testing::ElementsAre;
using testing::Property;
using testing::Return;
using testing::SetArgPointee;
using testing::Truly;

namespace content {

namespace {

const uint64_t kSignatureHeaderDate = 1564272000;  // 2019-07-28T00:00:00Z
const uint64_t kCertValidityPeriodEnforcementDate =
    1564617600;  // 2019-08-01T00:00:00Z
const int kOutputBufferSize = 4096;

constexpr char kTestSxgInnerURL[] = "https://test.example.org/test/";

bool IsCTSupported() {
#if BUILDFLAG(IS_CT_SUPPORTED)
  return true;
#else
  return false;
#endif
}

// "wildcard_example.org.public.pem.cbor" has dummy data in its "ocsp" field.
constexpr std::string_view kDummyOCSPDer = "OCSP";

class TestBrowserClient : public ContentBrowserClient {
  bool CanAcceptUntrustedExchangesIfNeeded() override { return true; }
};

std::string GetTestFileContents(std::string_view name) {
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
      SignedExchangeCertFetcher::CertificateCallback callback,
      SignedExchangeDevToolsProxy* devtools_proxy) override {
    EXPECT_EQ(cert_url, expected_cert_url_);

    auto cert_chain = SignedExchangeCertificateChain::Parse(
        base::as_bytes(base::make_span(cert_str_)), devtools_proxy);
    EXPECT_TRUE(cert_chain);

    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), SignedExchangeLoadResult::kSuccess,
                       std::move(cert_chain), net::IPAddress()));
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
    verify_result->Reset();
    return VerifyImpl(params, verify_result, out_req, net_log);
  }

  MOCK_METHOD4(VerifyImpl,
               int(const net::CertVerifier::RequestParams& params,
                   net::CertVerifyResult* verify_result,
                   std::unique_ptr<net::CertVerifier::Request>* out_req,
                   const net::NetLogWithSource& net_log));
  MOCK_METHOD1(SetConfig, void(const net::CertVerifier::Config& config));
  MOCK_METHOD1(AddObserver, void(Observer* observer));
  MOCK_METHOD1(RemoveObserver, void(Observer* observer));
};

class MockSCTAuditingDelegate : public net::SCTAuditingDelegate {
 public:
  MOCK_METHOD(bool, IsSCTAuditingEnabled, ());
  MOCK_METHOD(void,
              MaybeEnqueueReport,
              (const net::HostPortPair&,
               const net::X509Certificate*,
               const net::SignedCertificateTimestampAndStatusList&));
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
    return "application/signed-exchange;v=b3";
  }

  void SetUp() override {
    original_client_ = SetBrowserClientForTesting(&browser_client_);
    signed_exchange_utils::SetVerificationTimeForTesting(
        base::Time::UnixEpoch() + base::Seconds(kSignatureHeaderDate));

    source_stream_ = std::make_unique<net::MockSourceStream>();
    source_stream_->set_read_one_byte_at_a_time(true);
    source_ = source_stream_.get();
    cert_fetcher_factory_ =
        std::make_unique<MockSignedExchangeCertFetcherFactory>();
    mock_cert_fetcher_factory_ = cert_fetcher_factory_.get();
    mock_sct_auditing_delegate_ = std::make_unique<MockSCTAuditingDelegate>();
  }

  void TearDown() override {
    source_ = nullptr;
    mock_cert_fetcher_factory_ =nullptr;
    if (original_ignore_errors_spki_list_) {
      SignedExchangeCertificateChain::IgnoreErrorsSPKIList::
          SetInstanceForTesting(std::move(original_ignore_errors_spki_list_));
    }
    SignedExchangeHandler::SetNetworkContextForTesting(nullptr);
    network::NetworkContext::SetCertVerifierForTesting(nullptr);
    signed_exchange_utils::SetVerificationTimeForTesting(
        std::optional<base::Time>());
    SetBrowserClientForTesting(original_client_);
  }

  void SetCertVerifier(std::unique_ptr<net::CertVerifier> cert_verifier) {
    cert_verifier_ = std::move(cert_verifier);
    network::NetworkContext::SetCertVerifierForTesting(cert_verifier_.get());
  }

  void SetIgnoreCertificateErrorsSPKIList(const std::string value) {
    DCHECK(!original_ignore_errors_spki_list_);
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    command_line.AppendSwitchASCII(
        network::switches::kIgnoreCertificateErrorsSPKIList, value);
    original_ignore_errors_spki_list_ = SignedExchangeCertificateChain::
        IgnoreErrorsSPKIList::SetInstanceForTesting(
            std::make_unique<
                SignedExchangeCertificateChain::IgnoreErrorsSPKIList>(
                command_line));
  }

  // Creates a net::CertVerifyResult with some useful default values.
  net::CertVerifyResult CreateCertVerifyResult() {
    net::CertVerifyResult result;
    result.cert_status = net::OK;
    result.ocsp_result.response_status = bssl::OCSPVerifyResult::PROVIDED;
    result.ocsp_result.revocation_status = bssl::OCSPRevocationStatus::GOOD;
    // Return CT_POLICY_COMPLIES_VIA_SCTS by default. This may be overridden by
    // test cases.
    result.policy_compliance =
        net::ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS;
    return result;
  }

  // Sets up a MockCertVerifier that returns |result| for certificate in
  // |cert_file| and "test.example.org".
  void SetupMockCertVerifier(const std::string& cert_file,
                             net::CertVerifyResult result) {
    scoped_refptr<net::X509Certificate> original_cert =
        LoadCertificate(cert_file);
    result.verified_cert = original_cert;
    auto mock_cert_verifier = std::make_unique<net::MockCertVerifier>();
    mock_cert_verifier->AddResultForCertAndHost(
        original_cert, "test.example.org", result, net::OK);
    SetCertVerifier(std::move(mock_cert_verifier));
  }

  // Sets up |source_| stream with the contents of |file|.
  void SetSourceStreamContents(std::string_view file) {
    // MockSourceStream doesn't take ownership of the buffer, so we must keep it
    // alive.
    source_stream_contents_ = GetTestFileContents(file);
    source_->AddReadResult(source_stream_contents_.data(),
                           source_stream_contents_.size(), net::OK, GetParam());
    source_->AddReadResult(nullptr, 0, net::OK, GetParam());
  }

  // Reads from |stream| until an error occurs or the EOF is reached.
  // When an error occurs, returns the net error code. When an EOF is reached,
  // returns the number of bytes read. If |output| is non-null, appends data
  // read to it.
  int ReadStream(net::SourceStream* stream, std::string* output) {
    auto output_buffer =
        base::MakeRefCounted<net::IOBufferWithSize>(kOutputBufferSize);
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
  const network::mojom::URLResponseHead& resource_response() const {
    return *resource_response_;
  }

  // Creates a URLRequestContext that uses |mock_sct_auditing_delegate_|.
  std::unique_ptr<net::URLRequestContext> CreateTestURLRequestContext() {
    // We consume these mock objects, so register expectations beforehand.
    DCHECK(mock_sct_auditing_delegate_);
    auto context_builder = net::CreateTestURLRequestContextBuilder();
    context_builder->set_sct_auditing_delegate(
        std::move(mock_sct_auditing_delegate_));
    return context_builder->Build();
  }

  void CreateSignedExchangeHandler(
      std::unique_ptr<net::URLRequestContext> context) {
    url_request_context_ = std::move(context);
    network_context_ = std::make_unique<network::NetworkContext>(
        nullptr, network_context_remote_.BindNewPipeAndPassReceiver(),
        url_request_context_.get(),
        /*cors_exempt_header_list=*/std::vector<std::string>());
    SignedExchangeHandler::SetNetworkContextForTesting(network_context_.get());

    handler_ = std::make_unique<SignedExchangeHandler>(
        true /* is_secure_transport */, true /* has_nosniff */, ContentType(),
        std::move(source_stream_),
        base::BindOnce(&SignedExchangeHandlerTest::OnHeaderFound,
                       base::Unretained(this)),
        std::move(cert_fetcher_factory_),
        std::nullopt /* outer_request_isolation_info */, net::LOAD_NORMAL,
        net::IPEndPoint(),
        std::make_unique<blink::WebPackageRequestMatcher>(
            net::HttpRequestHeaders(), std::string() /* accept_langs */),
        nullptr /* devtools_proxy */, nullptr /* reporter */,
        FrameTreeNodeId());
  }

  void WaitForHeader() {
    while (!read_header()) {
      while (source_->awaiting_completion())
        source_->CompleteNextRead();
      task_environment_.RunUntilIdle();
    }
  }

  void ExpectHistogramValues(
      std::optional<SignedExchangeSignatureVerifier::Result> signature_result,
      std::optional<int32_t> cert_result,
      std::optional<net::ct::CTPolicyCompliance> ct_result,
      std::optional<bssl::OCSPVerifyResult::ResponseStatus>
          ocsp_response_status,
      std::optional<bssl::OCSPRevocationStatus> ocsp_revocation_status) {
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
  raw_ptr<MockSignedExchangeCertFetcherFactory>
      mock_cert_fetcher_factory_;
  std::unique_ptr<net::CertVerifier> cert_verifier_;
  std::unique_ptr<MockSCTAuditingDelegate> mock_sct_auditing_delegate_;
  raw_ptr<net::MockSourceStream> source_;
  std::unique_ptr<SignedExchangeHandler> handler_;

 private:
  void OnHeaderFound(SignedExchangeLoadResult result,
                     net::Error error,
                     const GURL& url,
                     network::mojom::URLResponseHeadPtr resource_response,
                     std::unique_ptr<net::SourceStream> payload_stream) {
    read_header_ = true;
    result_ = result;
    error_ = error;
    inner_url_ = url;
    resource_response_ = std::move(resource_response);
    payload_stream_ = std::move(payload_stream);
  }

  template <typename T>
  void ExpectZeroOrUniqueSample(const std::string& histogram_name,
                                std::optional<T> expected_value) {
    if (expected_value.has_value())
      histogram_tester_.ExpectUniqueSample(histogram_name, *expected_value, 1);
    else
      histogram_tester_.ExpectTotalCount(histogram_name, 0);
  }

  base::test::ScopedFeatureList feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  TestBrowserClient browser_client_;
  raw_ptr<ContentBrowserClient> original_client_;
  std::unique_ptr<net::URLRequestContext> url_request_context_;
  std::unique_ptr<network::NetworkContext> network_context_;
  mojo::Remote<network::mojom::NetworkContext> network_context_remote_;
  const url::Origin request_initiator_;
  std::unique_ptr<SignedExchangeCertificateChain::IgnoreErrorsSPKIList>
      original_ignore_errors_spki_list_;
  std::unique_ptr<net::MockSourceStream> source_stream_;
  std::unique_ptr<MockSignedExchangeCertFetcherFactory> cert_fetcher_factory_;

  bool read_header_ = false;
  SignedExchangeLoadResult result_;
  net::Error error_;
  GURL inner_url_;
  network::mojom::URLResponseHeadPtr resource_response_;
  std::unique_ptr<net::SourceStream> payload_stream_;
  std::string source_stream_contents_;
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
  SetupMockCertVerifier("prime256v1-sha256.public.pem",
                        CreateCertVerifyResult());
  SetSourceStreamContents("test.example.org_test.sxg");

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
      /*ct_result=*/
      IsCTSupported() ? net::ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS
                      : net::ct::CTPolicyCompliance::
                            CT_POLICY_COMPLIANCE_DETAILS_NOT_AVAILABLE,
      bssl::OCSPVerifyResult::PROVIDED, bssl::OCSPRevocationStatus::GOOD);
}

TEST_P(SignedExchangeHandlerTest, MimeType) {
  mock_cert_fetcher_factory_->ExpectFetch(
      GURL("https://cert.example.org/cert.msg"),
      GetTestFileContents("test.example.org.public.pem.cbor"));
  SetupMockCertVerifier("prime256v1-sha256.public.pem",
                        CreateCertVerifyResult());
  SetSourceStreamContents("test.example.org_hello.txt.sxg");

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

TEST_P(SignedExchangeHandlerTest, AdditionalContentEncodingShouldBeRejected) {
  mock_cert_fetcher_factory_->ExpectFetch(
      GURL("https://cert.example.org/cert.msg"),
      GetTestFileContents("test.example.org.public.pem.cbor"));
  SetupMockCertVerifier("prime256v1-sha256.public.pem",
                        CreateCertVerifyResult());
  SetSourceStreamContents("test.example.org_test.html.gz.sxg");

  CreateSignedExchangeHandler(CreateTestURLRequestContext());
  WaitForHeader();

  ASSERT_TRUE(read_header());
  EXPECT_EQ(SignedExchangeLoadResult::kInvalidIntegrityHeader, result());
  EXPECT_EQ(net::ERR_INVALID_SIGNED_EXCHANGE, error());
  EXPECT_EQ(kTestSxgInnerURL, inner_url());
  // Drain the MockSourceStream, otherwise its destructer causes DCHECK failure.
  ReadStream(source_, nullptr);
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
  SetupMockCertVerifier("prime256v1-sha256-noext.public.pem",
                        CreateCertVerifyResult());
  SetSourceStreamContents("test.example.org_noext_test.sxg");

  CreateSignedExchangeHandler(CreateTestURLRequestContext());
  WaitForHeader();

  ASSERT_TRUE(read_header());
  EXPECT_EQ(SignedExchangeLoadResult::kCertRequirementsNotMet, result());
  EXPECT_EQ(net::ERR_INVALID_SIGNED_EXCHANGE, error());
  EXPECT_EQ(kTestSxgInnerURL, inner_url());
  // Drain the MockSourceStream, otherwise its destructer causes DCHECK failure.
  ReadStream(source_, nullptr);
}

TEST_P(SignedExchangeHandlerTest, CertValidMoreThan90DaysShouldBeRejected) {
  mock_cert_fetcher_factory_->ExpectFetch(
      GURL("https://cert.example.org/cert.msg"),
      GetTestFileContents(
          "test.example.org-validity-too-long.public.pem.cbor"));
  SetupMockCertVerifier("prime256v1-sha256-validity-too-long.public.pem",
                        CreateCertVerifyResult());
  SetSourceStreamContents("test.example.org_cert_validity_too_long.sxg");

  CreateSignedExchangeHandler(CreateTestURLRequestContext());
  WaitForHeader();

  ASSERT_TRUE(read_header());
  EXPECT_EQ(SignedExchangeLoadResult::kCertValidityPeriodTooLong, result());
  EXPECT_EQ(net::ERR_INVALID_SIGNED_EXCHANGE, error());
  EXPECT_EQ(kTestSxgInnerURL, inner_url());
  // Drain the MockSourceStream, otherwise its destructer causes DCHECK failure.
  ReadStream(source_, nullptr);
}

TEST_P(SignedExchangeHandlerTest,
       CertValidMoreThan90DaysShouldBeAllowedByIgnoreErrorsSPKIListFlag) {
  SetIgnoreCertificateErrorsSPKIList(kPEMECDSAP256SPKIHash);

  signed_exchange_utils::SetVerificationTimeForTesting(
      base::Time::UnixEpoch() +
      base::Seconds(kCertValidityPeriodEnforcementDate));
  mock_cert_fetcher_factory_->ExpectFetch(
      GURL("https://cert.example.org/cert.msg"),
      GetTestFileContents("test.example.org.public.pem.cbor"));
  SetupMockCertVerifier("prime256v1-sha256.public.pem",
                        CreateCertVerifyResult());
  SetSourceStreamContents("test.example.org_test.sxg");

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

TEST_P(SignedExchangeHandlerTest,
       CertWithoutExtensionAllowedByIgnoreErrorsSPKIListFlag) {
  SetIgnoreCertificateErrorsSPKIList(kPEMECDSAP256SPKIHash);

  mock_cert_fetcher_factory_->ExpectFetch(
      GURL("https://cert.example.org/cert.msg"),
      GetTestFileContents("test.example.org-noext.public.pem.cbor"));
  SetupMockCertVerifier("prime256v1-sha256-noext.public.pem",
                        CreateCertVerifyResult());
  SetSourceStreamContents("test.example.org_noext_test.sxg");

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

  SetSourceStreamContents("test.example.org_test.sxg");

  CreateSignedExchangeHandler(CreateTestURLRequestContext());
  WaitForHeader();

  ASSERT_TRUE(read_header());
  EXPECT_EQ(SignedExchangeLoadResult::kSignatureVerificationError, result());
  EXPECT_EQ(net::ERR_INVALID_SIGNED_EXCHANGE, error());
  EXPECT_EQ(kTestSxgInnerURL, inner_url());
  ExpectHistogramValues(
      SignedExchangeSignatureVerifier::Result::kErrCertificateSHA256Mismatch,
      std::nullopt /* cert_result */, std::nullopt /* ct_result */,
      std::nullopt /* ocsp_response_status */,
      std::nullopt /* ocsp_revocation_status */);

  // Drain the MockSourceStream, otherwise its destructer causes DCHECK failure.
  ReadStream(source_, nullptr);
}

TEST_P(SignedExchangeHandlerTest, VerifyCertFailure) {
  mock_cert_fetcher_factory_->ExpectFetch(
      GURL("https://cert.example.org/cert.msg"),
      GetTestFileContents("test.example.org.public.pem.cbor"));
  SetupMockCertVerifier("prime256v1-sha256.public.pem",
                        CreateCertVerifyResult());

  // The certificate is for "test.example.org". But the request URL of the sxg
  // file is "https://test.example.com/test/". So the certification verification
  // must fail.
  SetSourceStreamContents("test.example.com_invalid_test.sxg");

  CreateSignedExchangeHandler(CreateTestURLRequestContext());
  WaitForHeader();

  ASSERT_TRUE(read_header());
  EXPECT_EQ(SignedExchangeLoadResult::kCertVerificationError, result());
  EXPECT_EQ(net::ERR_INVALID_SIGNED_EXCHANGE, error());
  EXPECT_EQ("https://test.example.com/test/", inner_url());
  ExpectHistogramValues(
      SignedExchangeSignatureVerifier::Result::kSuccess, net::ERR_CERT_INVALID,
      net::ct::CTPolicyCompliance::CT_POLICY_COMPLIANCE_DETAILS_NOT_AVAILABLE,
      std::nullopt /* ocsp_response_status */,
      std::nullopt /* ocsp_revocation_status */);

  // Drain the MockSourceStream, otherwise its destructer causes DCHECK failure.
  ReadStream(source_, nullptr);
}

TEST_P(SignedExchangeHandlerTest, OCSPNotChecked) {
  mock_cert_fetcher_factory_->ExpectFetch(
      GURL("https://cert.example.org/cert.msg"),
      GetTestFileContents("test.example.org.public.pem.cbor"));
  net::CertVerifyResult cert_result = CreateCertVerifyResult();
  cert_result.ocsp_result.response_status = bssl::OCSPVerifyResult::NOT_CHECKED;
  SetupMockCertVerifier("prime256v1-sha256.public.pem", cert_result);
  SetSourceStreamContents("test.example.org_test.sxg");

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
  net::CertVerifyResult cert_result = CreateCertVerifyResult();
  cert_result.ocsp_result.response_status = bssl::OCSPVerifyResult::MISSING;
  SetupMockCertVerifier("prime256v1-sha256.public.pem", cert_result);
  SetSourceStreamContents("test.example.org_test.sxg");

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
  net::CertVerifyResult cert_result = CreateCertVerifyResult();
  cert_result.ocsp_result.response_status =
      bssl::OCSPVerifyResult::INVALID_DATE;
  SetupMockCertVerifier("prime256v1-sha256.public.pem", cert_result);
  SetSourceStreamContents("test.example.org_test.sxg");

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
  net::CertVerifyResult cert_result = CreateCertVerifyResult();
  cert_result.ocsp_result.response_status = bssl::OCSPVerifyResult::PROVIDED;
  cert_result.ocsp_result.revocation_status =
      bssl::OCSPRevocationStatus::REVOKED;
  SetupMockCertVerifier("prime256v1-sha256.public.pem", cert_result);
  SetSourceStreamContents("test.example.org_test.sxg");

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
  fake_result.ocsp_result.response_status = bssl::OCSPVerifyResult::PROVIDED;
  fake_result.ocsp_result.revocation_status = bssl::OCSPRevocationStatus::GOOD;

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

  SetSourceStreamContents("test.example.org_test.sxg");

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
  if (!IsCTSupported()) {
    GTEST_SKIP() << "CT not supported";
  }

  mock_cert_fetcher_factory_->ExpectFetch(
      GURL("https://cert.example.org/cert.msg"),
      GetTestFileContents("test.example.org.public.pem.cbor"));

  net::CertVerifyResult cert_result = CreateCertVerifyResult();
  cert_result.is_issued_by_known_root = true;
  cert_result.policy_compliance =
      net::ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS;
  SetupMockCertVerifier("prime256v1-sha256.public.pem", cert_result);


  SetSourceStreamContents("test.example.org_test.sxg");

  CreateSignedExchangeHandler(CreateTestURLRequestContext());
  WaitForHeader();

  ASSERT_TRUE(read_header());
  EXPECT_EQ(SignedExchangeLoadResult::kCTVerificationError, result());
  EXPECT_EQ(net::ERR_INVALID_SIGNED_EXCHANGE, error());
  EXPECT_EQ(kTestSxgInnerURL, inner_url());
  ExpectHistogramValues(SignedExchangeSignatureVerifier::Result::kSuccess,
                        net::ERR_CERTIFICATE_TRANSPARENCY_REQUIRED,
                        net::ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS,
                        std::nullopt /* ocsp_response_status */,
                        std::nullopt /* ocsp_revocation_status */);
  // Drain the MockSourceStream, otherwise its destructer causes DCHECK failure.
  ReadStream(source_, nullptr);
}

TEST_P(SignedExchangeHandlerTest, CTRequirementsMetForPubliclyTrustedCert) {
  if (!IsCTSupported()) {
    GTEST_SKIP() << "CT not supported";
  }

  mock_cert_fetcher_factory_->ExpectFetch(
      GURL("https://cert.example.org/cert.msg"),
      GetTestFileContents("test.example.org.public.pem.cbor"));

  net::CertVerifyResult cert_result = CreateCertVerifyResult();
  cert_result.is_issued_by_known_root = true;
  cert_result.cert_status = net::CERT_STATUS_IS_EV;
  SetupMockCertVerifier("prime256v1-sha256.public.pem", cert_result);

  // The mock CT policy enforcer will return CT_POLICY_COMPLIES_VIA_SCTS, as
  // configured in SetUp().

  SetSourceStreamContents("test.example.org_test.sxg");

  CreateSignedExchangeHandler(CreateTestURLRequestContext());
  WaitForHeader();

  ASSERT_TRUE(read_header());
  EXPECT_EQ(SignedExchangeLoadResult::kSuccess, result());
  EXPECT_EQ(net::OK, error());
  // EV status should be preserved.
  EXPECT_TRUE(resource_response().ssl_info->cert_status &
              net::CERT_STATUS_IS_EV);
  EXPECT_EQ(net::ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS,
            resource_response().ssl_info->ct_policy_compliance);
  ExpectHistogramValues(
      SignedExchangeSignatureVerifier::Result::kSuccess, net::OK,
      net::ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS,
      bssl::OCSPVerifyResult::PROVIDED, bssl::OCSPRevocationStatus::GOOD);

  std::string payload;
  int rv = ReadPayloadStream(&payload);
  std::string expected_payload = GetTestFileContents("test.html");
  EXPECT_EQ(expected_payload, payload);
  EXPECT_EQ(static_cast<int>(expected_payload.size()), rv);
}

TEST_P(SignedExchangeHandlerTest, CTNotRequiredForLocalAnchors) {
  if (!IsCTSupported()) {
    GTEST_SKIP() << "CT not supported";
  }
  mock_cert_fetcher_factory_->ExpectFetch(
      GURL("https://cert.example.org/cert.msg"),
      GetTestFileContents("test.example.org.public.pem.cbor"));

  net::CertVerifyResult cert_result = CreateCertVerifyResult();
  cert_result.is_issued_by_known_root = false;  // Local anchor.
  cert_result.policy_compliance =
      net::ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS;
  SetupMockCertVerifier("prime256v1-sha256.public.pem", cert_result);

  SetSourceStreamContents("test.example.org_test.sxg");

  CreateSignedExchangeHandler(CreateTestURLRequestContext());
  WaitForHeader();

  ASSERT_TRUE(read_header());
  EXPECT_EQ(SignedExchangeLoadResult::kSuccess, result());
  EXPECT_EQ(net::OK, error());
  EXPECT_EQ(net::ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS,
            resource_response().ssl_info->ct_policy_compliance);
  ExpectHistogramValues(
      SignedExchangeSignatureVerifier::Result::kSuccess, net::OK,
      net::ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS,
      bssl::OCSPVerifyResult::PROVIDED, bssl::OCSPRevocationStatus::GOOD);

  std::string payload;
  int rv = ReadPayloadStream(&payload);
  std::string expected_payload = GetTestFileContents("test.html");
  EXPECT_EQ(expected_payload, payload);
  EXPECT_EQ(static_cast<int>(expected_payload.size()), rv);
}

// Test that SignedExchangeHandler calls CTPolicyEnforcer with appropriate
// arguments.
TEST_P(SignedExchangeHandlerTest, CTVerifierParams) {
  if (!IsCTSupported()) {
    GTEST_SKIP() << "CT not supported";
  }

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

  // Mock a verify result including the SCTs.
  auto verify_result = CreateCertVerifyResult();
  verify_result.scts = fake_sct_list;
  SetupMockCertVerifier("prime256v1-sha256.public.pem", verify_result);

  std::string contents = GetTestFileContents("test.example.org_test.sxg");
  source_->AddReadResult(contents.data(), contents.size(), net::OK,
                         net::MockSourceStream::ASYNC);
  source_->AddReadResult(nullptr, 0, net::OK, net::MockSourceStream::ASYNC);

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

// Test that SignedExchangeHandler calls SCTAuditingDelegate to enqueue reports.
TEST_P(SignedExchangeHandlerTest, SCTAuditingReportEnqueued) {
  if (!IsCTSupported()) {
    GTEST_SKIP() << "CT not supported";
  }
  mock_cert_fetcher_factory_->ExpectFetch(
      GURL("https://cert.example.org/cert.msg"),
      GetTestFileContents("test.example.org.public.pem.cbor"));

  net::CertVerifyResult cert_result = CreateCertVerifyResult();
  cert_result.is_issued_by_known_root = true;
  SetupMockCertVerifier("prime256v1-sha256.public.pem", cert_result);

  // The mock cert verifier will return CT_POLICY_COMPLIES_VIA_SCTS, as
  // configured in CreateCertVerifyResult().

  // Add SCTAuditingDelegate mock results.
  EXPECT_CALL(*mock_sct_auditing_delegate_, IsSCTAuditingEnabled())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_sct_auditing_delegate_, MaybeEnqueueReport(_, _, _))
      .Times(1);

  SetSourceStreamContents("test.example.org_test.sxg");

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

INSTANTIATE_TEST_SUITE_P(SignedExchangeHandlerTests,
                         SignedExchangeHandlerTest,
                         ::testing::Values(net::MockSourceStream::SYNC,
                                           net::MockSourceStream::ASYNC));

}  // namespace content
