// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/expect_ct_reporter.h"

#include <string>

#include "base/base64.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "net/cert/ct_serialization.h"
#include "net/cert/signed_certificate_timestamp_and_status.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/test_data_directory.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/report_sender.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/public/cpp/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace network {
namespace {

const char kSendHistogramName[] = "SSL.ExpectCTReportSendingAttempt";
const char kFailureHistogramName[] = "SSL.ExpectCTReportFailure2";

// A test ReportSender that exposes the latest report URI and
// serialized report to be sent.
class TestCertificateReportSender : public net::ReportSender {
 public:
  TestCertificateReportSender()
      : ReportSender(nullptr, TRAFFIC_ANNOTATION_FOR_TESTS) {}
  ~TestCertificateReportSender() override {}

  void Send(const GURL& report_uri,
            base::StringPiece content_type,
            base::StringPiece serialized_report,
            const base::Callback<void()>& success_callback,
            const base::Callback<void(const GURL&, int, int)>& error_callback)
      override {
    sent_report_count_++;
    latest_report_uri_ = report_uri;
    serialized_report.CopyToString(&latest_serialized_report_);
    content_type.CopyToString(&latest_content_type_);
    if (!report_callback_.is_null()) {
      EXPECT_EQ(expected_report_uri_, latest_report_uri_);
      report_callback_.Run();
    }
  }

  int sent_report_count() const { return sent_report_count_; }

  const GURL& latest_report_uri() const { return latest_report_uri_; }

  const std::string& latest_content_type() const {
    return latest_content_type_;
  }

  const std::string& latest_serialized_report() const {
    return latest_serialized_report_;
  }

  // Can be called to wait for a single report, which is expected to be sent to
  // |report_uri|. Returns immediately if a report has already been sent in the
  // past.
  void WaitForReport(const GURL& report_uri) {
    if (!latest_report_uri_.is_empty()) {
      EXPECT_EQ(report_uri, latest_report_uri_);
      return;
    }
    base::RunLoop run_loop;
    report_callback_ = run_loop.QuitClosure();
    expected_report_uri_ = report_uri;
    run_loop.Run();
  }

 private:
  int sent_report_count_ = 0;
  GURL latest_report_uri_;
  std::string latest_content_type_;
  std::string latest_serialized_report_;
  base::Closure report_callback_;
  GURL expected_report_uri_;
};

// Constructs a net::SignedCertificateTimestampAndStatus with the given
// information and appends it to |sct_list|.
void MakeTestSCTAndStatus(
    net::ct::SignedCertificateTimestamp::Origin origin,
    const std::string& log_id,
    const std::string& extensions,
    const std::string& signature_data,
    const base::Time& timestamp,
    net::ct::SCTVerifyStatus status,
    net::SignedCertificateTimestampAndStatusList* sct_list) {
  scoped_refptr<net::ct::SignedCertificateTimestamp> sct(
      new net::ct::SignedCertificateTimestamp());
  sct->version = net::ct::SignedCertificateTimestamp::V1;
  sct->log_id = log_id;
  sct->extensions = extensions;
  sct->timestamp = timestamp;
  sct->signature.signature_data = signature_data;
  sct->origin = origin;
  sct_list->push_back(net::SignedCertificateTimestampAndStatus(sct, status));
}

// Checks that |expected_cert| matches the PEM-encoded certificate chain
// in |chain|.
void CheckReportCertificateChain(
    const scoped_refptr<net::X509Certificate>& expected_cert,
    const base::ListValue& chain) {
  std::vector<std::string> pem_encoded_chain;
  expected_cert->GetPEMEncodedChain(&pem_encoded_chain);
  ASSERT_EQ(pem_encoded_chain.size(), chain.GetSize());

  for (size_t i = 0; i < pem_encoded_chain.size(); i++) {
    std::string cert_pem;
    ASSERT_TRUE(chain.GetString(i, &cert_pem));
    EXPECT_EQ(pem_encoded_chain[i], cert_pem);
  }
}

// Converts the string value of a reported SCT's origin to a
// net::ct::SignedCertificateTimestamp::Origin value.
net::ct::SignedCertificateTimestamp::Origin SCTOriginStringToOrigin(
    const std::string& origin_string) {
  if (origin_string == "embedded")
    return net::ct::SignedCertificateTimestamp::SCT_EMBEDDED;
  if (origin_string == "tls-extension")
    return net::ct::SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION;
  if (origin_string == "ocsp")
    return net::ct::SignedCertificateTimestamp::SCT_FROM_OCSP_RESPONSE;
  NOTREACHED();
  return net::ct::SignedCertificateTimestamp::SCT_EMBEDDED;
}

// Checks that an SCT |sct| appears with status |status| in |report_list|, a
// list of SCTs from an Expect-CT report.
::testing::AssertionResult FindSCTInReportList(
    const scoped_refptr<net::ct::SignedCertificateTimestamp>& expected_sct,
    net::ct::SCTVerifyStatus expected_status,
    const base::ListValue& report_list) {
  std::string expected_serialized_sct;
  if (!net::ct::EncodeSignedCertificateTimestamp(expected_sct,
                                                 &expected_serialized_sct)) {
    return ::testing::AssertionFailure() << "Failed to serialize SCT";
  }

  for (size_t i = 0; i < report_list.GetSize(); i++) {
    const base::DictionaryValue* report_sct;
    if (!report_list.GetDictionary(i, &report_sct)) {
      return ::testing::AssertionFailure()
             << "Failed to get dictionary value from report SCT list";
    }

    std::string serialized_sct;
    EXPECT_TRUE(report_sct->GetString("serialized_sct", &serialized_sct));
    std::string decoded_serialized_sct;
    EXPECT_TRUE(base::Base64Decode(serialized_sct, &decoded_serialized_sct));
    if (decoded_serialized_sct != expected_serialized_sct)
      continue;

    std::string source;
    EXPECT_TRUE(report_sct->GetString("source", &source));
    EXPECT_EQ(expected_sct->origin, SCTOriginStringToOrigin(source));

    std::string report_status;
    EXPECT_TRUE(report_sct->GetString("status", &report_status));
    switch (expected_status) {
      case net::ct::SCT_STATUS_LOG_UNKNOWN:
        EXPECT_EQ("unknown", report_status);
        break;
      case net::ct::SCT_STATUS_INVALID_SIGNATURE:
      case net::ct::SCT_STATUS_INVALID_TIMESTAMP: {
        EXPECT_EQ("invalid", report_status);
        break;
      }
      case net::ct::SCT_STATUS_OK: {
        EXPECT_EQ("valid", report_status);
        break;
      }
      case net::ct::SCT_STATUS_NONE:
        NOTREACHED();
    }
    return ::testing::AssertionSuccess();
  }

  return ::testing::AssertionFailure() << "Failed to find SCT in report list";
}

// Checks that all |expected_scts| appears in the given lists of SCTs
// from an Expect CT report.
void CheckReportSCTs(
    const net::SignedCertificateTimestampAndStatusList& expected_scts,
    const base::ListValue& scts) {
  EXPECT_EQ(expected_scts.size(), scts.GetSize());
  for (const auto& expected_sct : expected_scts) {
    ASSERT_TRUE(
        FindSCTInReportList(expected_sct.sct, expected_sct.status, scts));
  }
}

// Checks that the |serialized_report| deserializes properly and
// contains the correct information (hostname, port, served and
// validated certificate chains, SCTs) for the given |host_port| and
// |ssl_info|.
void CheckExpectCTReport(const std::string& serialized_report,
                         const net::HostPortPair& host_port,
                         const std::string& expiration,
                         const net::SSLInfo& ssl_info) {
  std::unique_ptr<base::Value> value(
      base::JSONReader::ReadDeprecated(serialized_report));
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->is_dict());

  base::DictionaryValue* outer_report_dict;
  ASSERT_TRUE(value->GetAsDictionary(&outer_report_dict));

  base::DictionaryValue* report_dict;
  ASSERT_TRUE(
      outer_report_dict->GetDictionary("expect-ct-report", &report_dict));

  std::string report_hostname;
  EXPECT_TRUE(report_dict->GetString("hostname", &report_hostname));
  EXPECT_EQ(host_port.host(), report_hostname);
  int report_port;
  EXPECT_TRUE(report_dict->GetInteger("port", &report_port));
  EXPECT_EQ(host_port.port(), report_port);

  std::string report_expiration;
  EXPECT_TRUE(
      report_dict->GetString("effective-expiration-date", &report_expiration));
  EXPECT_EQ(expiration, report_expiration);

  const base::ListValue* report_served_certificate_chain = nullptr;
  ASSERT_TRUE(report_dict->GetList("served-certificate-chain",
                                   &report_served_certificate_chain));
  ASSERT_NO_FATAL_FAILURE(CheckReportCertificateChain(
      ssl_info.unverified_cert, *report_served_certificate_chain));

  const base::ListValue* report_validated_certificate_chain = nullptr;
  ASSERT_TRUE(report_dict->GetList("validated-certificate-chain",
                                   &report_validated_certificate_chain));
  ASSERT_NO_FATAL_FAILURE(CheckReportCertificateChain(
      ssl_info.cert, *report_validated_certificate_chain));

  const base::ListValue* report_scts = nullptr;
  ASSERT_TRUE(report_dict->GetList("scts", &report_scts));

  ASSERT_NO_FATAL_FAILURE(
      CheckReportSCTs(ssl_info.signed_certificate_timestamps, *report_scts));
}

// A test network delegate that allows the user to specify a callback to
// be run whenever a net::URLRequest is destroyed.
class TestExpectCTNetworkDelegate : public net::NetworkDelegateImpl {
 public:
  TestExpectCTNetworkDelegate()
      : url_request_destroyed_callback_(base::Closure()) {}

  void set_url_request_destroyed_callback(const base::Closure& callback) {
    url_request_destroyed_callback_ = callback;
  }

  // net::NetworkDelegateImpl:
  void OnURLRequestDestroyed(net::URLRequest* request) override {
    url_request_destroyed_callback_.Run();
  }

 private:
  base::Closure url_request_destroyed_callback_;

  DISALLOW_COPY_AND_ASSIGN(TestExpectCTNetworkDelegate);
};

// A test fixture that allows tests to send a report and wait until the
// net::URLRequest that sent the report is destroyed.
class ExpectCTReporterWaitTest : public ::testing::Test {
 public:
  ExpectCTReporterWaitTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}

  void SetUp() override {
    // Initializes URLRequestContext after the thread is set up.
    context_.reset(new net::TestURLRequestContext(true));
    context_->set_network_delegate(&network_delegate_);
    context_->Init();
    net::URLRequestFailedJob::AddUrlHandler();
  }

  void TearDown() override {
    net::URLRequestFilter::GetInstance()->ClearHandlers();
  }

  net::TestURLRequestContext* context() { return context_.get(); }

 protected:
  void SendReport(ExpectCTReporter* reporter,
                  const net::HostPortPair& host_port,
                  const GURL& report_uri,
                  base::Time expiration,
                  const net::SSLInfo& ssl_info) {
    base::RunLoop run_loop;
    network_delegate_.set_url_request_destroyed_callback(
        run_loop.QuitClosure());
    reporter->OnExpectCTFailed(
        host_port, report_uri, expiration, ssl_info.cert.get(),
        ssl_info.unverified_cert.get(), ssl_info.signed_certificate_timestamps);
    run_loop.Run();
  }

 private:
  TestExpectCTNetworkDelegate network_delegate_;
  std::unique_ptr<net::TestURLRequestContext> context_;
  base::test::TaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(ExpectCTReporterWaitTest);
};

std::unique_ptr<net::test_server::HttpResponse> ReplyToPostWith200(
    const net::test_server::HttpRequest& request) {
  if (request.method != net::test_server::METHOD_POST)
    return nullptr;

  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  return http_response;
}

std::unique_ptr<net::test_server::HttpResponse> HandleReportPreflight(
    const std::map<std::string, std::string>& cors_headers,
    base::RepeatingClosure callback,
    const net::test_server::HttpRequest& request) {
  if (request.method != net::test_server::METHOD_OPTIONS)
    return nullptr;

  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  for (const auto& cors_header : cors_headers) {
    http_response->AddCustomHeader(cors_header.first, cors_header.second);
  }

  if (!callback.is_null()) {
    callback.Run();
  }

  return http_response;
}

std::unique_ptr<net::test_server::HttpResponse> HandleReportPreflightForPath(
    const std::string& path,
    const std::map<std::string, std::string>& cors_headers,
    base::RepeatingClosure callback,
    const net::test_server::HttpRequest& request) {
  if (request.relative_url != path)
    return nullptr;
  return HandleReportPreflight(cors_headers, callback, request);
}

// A test fixture that responds properly to CORS preflights so that reports can
// be successfully sent to test_server().
class ExpectCTReporterTest : public ::testing::Test {
 public:
  const std::map<std::string, std::string> kGoodCorsHeaders{
      {"Access-Control-Allow-Origin", "*"},
      {"Access-Control-Allow-Methods", "GET,POST"},
      {"Access-Control-Allow-Headers", "content-type,another-header"}};

  ExpectCTReporterTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}
  ~ExpectCTReporterTest() override {}

 protected:
  net::EmbeddedTestServer& test_server() { return report_server_; }

  // Tests that reports are not sent when the CORS preflight request returns the
  // header field |preflight_header_name| with value given by
  // |preflight_header_bad_value|, and that reports are successfully sent when
  // it has value given by |preflight_header_good_value|.
  void TestForReportPreflightFailure(
      ExpectCTReporter* reporter,
      TestCertificateReportSender* sender,
      const net::HostPortPair& host_port,
      const net::SSLInfo& ssl_info,
      const std::string& preflight_header_name,
      const std::string& preflight_header_bad_value,
      const std::string& preflight_header_good_value) {
    const std::string fail_path = "/report1";
    const std::string successful_path = "/report2";

    std::map<std::string, std::string> bad_cors_headers = kGoodCorsHeaders;
    bad_cors_headers[preflight_header_name] = preflight_header_bad_value;
    std::map<std::string, std::string> good_cors_headers = kGoodCorsHeaders;
    good_cors_headers[preflight_header_name] = preflight_header_good_value;

    base::RunLoop bad_cors_run_loop;
    report_server_.RegisterRequestHandler(
        base::Bind(&HandleReportPreflightForPath, fail_path, bad_cors_headers,
                   bad_cors_run_loop.QuitClosure()));
    report_server_.RegisterRequestHandler(
        base::Bind(&HandleReportPreflightForPath, successful_path,
                   good_cors_headers, base::RepeatingClosure()));
    ASSERT_TRUE(report_server_.Start());

    const GURL fail_report_uri = test_server().GetURL(fail_path);
    const GURL successful_report_uri = test_server().GetURL(successful_path);

    reporter->OnExpectCTFailed(
        host_port, fail_report_uri, base::Time(), ssl_info.cert.get(),
        ssl_info.unverified_cert.get(), ssl_info.signed_certificate_timestamps);
    bad_cors_run_loop.Run();
    // The CORS preflight response may not even have been received yet, so
    // these expectations are mostly aspirational.
    EXPECT_TRUE(sender->latest_report_uri().is_empty());
    EXPECT_TRUE(sender->latest_serialized_report().empty());

    // Send a report to the url with good CORS headers. The test will fail
    // if the previous OnExpectCTFailed() call unexpectedly resulted in a
    // report, as WaitForReport() would see the previous report to /report1
    // instead of the expected report to /report2, or sent_report_count() will
    // be 2.
    reporter->OnExpectCTFailed(
        host_port, successful_report_uri, base::Time(), ssl_info.cert.get(),
        ssl_info.unverified_cert.get(), ssl_info.signed_certificate_timestamps);
    sender->WaitForReport(successful_report_uri);
    EXPECT_EQ(successful_report_uri, sender->latest_report_uri());
    EXPECT_EQ(1, sender->sent_report_count());
  }

 private:
  base::test::TaskEnvironment task_environment_;
  net::EmbeddedTestServer report_server_;
};

}  // namespace

// Test that no report is sent when the feature is not enabled.
TEST_F(ExpectCTReporterTest, FeatureDisabled) {
  test_server().RegisterRequestHandler(base::Bind(
      &HandleReportPreflight, kGoodCorsHeaders, base::RepeatingClosure()));
  ASSERT_TRUE(test_server().Start());

  base::HistogramTester histograms;
  histograms.ExpectTotalCount(kSendHistogramName, 0);

  TestCertificateReportSender* sender = new TestCertificateReportSender();
  net::TestURLRequestContext context;
  ExpectCTReporter reporter(&context, base::Closure(), base::Closure());
  reporter.report_sender_.reset(sender);
  EXPECT_TRUE(sender->latest_report_uri().is_empty());
  EXPECT_TRUE(sender->latest_serialized_report().empty());

  net::SSLInfo ssl_info;
  ssl_info.cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  ssl_info.unverified_cert = net::ImportCertFromFile(
      net::GetTestCertsDirectory(), "localhost_cert.pem");

  net::HostPortPair host_port("example.test", 443);

  {
    const GURL report_uri = test_server().GetURL("/report1");
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndDisableFeature(features::kExpectCTReporting);

    reporter.OnExpectCTFailed(
        host_port, report_uri, base::Time(), ssl_info.cert.get(),
        ssl_info.unverified_cert.get(), ssl_info.signed_certificate_timestamps);
    EXPECT_TRUE(sender->latest_report_uri().is_empty());
    EXPECT_TRUE(sender->latest_serialized_report().empty());

    histograms.ExpectTotalCount(kSendHistogramName, 0);
  }

  // Enable the feature and send a dummy report. The test will fail if the
  // previous OnExpectCTFailed() call unexpectedly resulted in a report, as the
  // WaitForReport() would see the previous report to /report1 instead of the
  // expected report to /report2.
  {
    const GURL report_uri = test_server().GetURL("/report2");
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeature(features::kExpectCTReporting);
    reporter.OnExpectCTFailed(
        host_port, report_uri, base::Time(), ssl_info.cert.get(),
        ssl_info.unverified_cert.get(), ssl_info.signed_certificate_timestamps);
    sender->WaitForReport(report_uri);
    EXPECT_EQ(report_uri, sender->latest_report_uri());
    EXPECT_EQ(1, sender->sent_report_count());
  }
}

// Test that no report is sent if the report URI is empty.
TEST_F(ExpectCTReporterTest, EmptyReportURI) {
  base::HistogramTester histograms;
  histograms.ExpectTotalCount(kSendHistogramName, 0);

  TestCertificateReportSender* sender = new TestCertificateReportSender();
  net::TestURLRequestContext context;
  ExpectCTReporter reporter(&context, base::Closure(), base::Closure());
  reporter.report_sender_.reset(sender);
  EXPECT_TRUE(sender->latest_report_uri().is_empty());
  EXPECT_TRUE(sender->latest_serialized_report().empty());

  reporter.OnExpectCTFailed(net::HostPortPair(), GURL(), base::Time(), nullptr,
                            nullptr,
                            net::SignedCertificateTimestampAndStatusList());
  EXPECT_TRUE(sender->latest_report_uri().is_empty());
  EXPECT_TRUE(sender->latest_serialized_report().empty());

  histograms.ExpectTotalCount(kSendHistogramName, 0);
}

// Test that if a report fails to send, the UMA metric is recorded.
TEST_F(ExpectCTReporterWaitTest, SendReportFailure) {
  base::HistogramTester histograms;
  histograms.ExpectTotalCount(kFailureHistogramName, 0);
  histograms.ExpectTotalCount(kSendHistogramName, 0);

  ExpectCTReporter reporter(context(), base::Closure(), base::Closure());

  net::SSLInfo ssl_info;
  ssl_info.cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  ssl_info.unverified_cert = net::ImportCertFromFile(
      net::GetTestCertsDirectory(), "localhost_cert.pem");

  net::HostPortPair host_port("example.test", 443);
  GURL report_uri(
      net::URLRequestFailedJob::GetMockHttpUrl(net::ERR_CONNECTION_FAILED));

  SendReport(&reporter, host_port, report_uri, base::Time(), ssl_info);

  histograms.ExpectTotalCount(kFailureHistogramName, 1);
  histograms.ExpectBucketCount(kFailureHistogramName,
                               -net::ERR_CONNECTION_FAILED, 1);
  histograms.ExpectTotalCount(kSendHistogramName, 1);
  histograms.ExpectBucketCount(kSendHistogramName, true, 1);
}

// Test that if a report fails to send, the failure callback is called.
TEST_F(ExpectCTReporterWaitTest, SendReportFailureCallback) {
  base::RunLoop run_loop;
  ExpectCTReporter reporter(context(), base::Closure(), run_loop.QuitClosure());

  net::SSLInfo ssl_info;
  ssl_info.cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  ssl_info.unverified_cert = net::ImportCertFromFile(
      net::GetTestCertsDirectory(), "localhost_cert.pem");

  net::HostPortPair host_port("example.test", 443);
  GURL report_uri(
      net::URLRequestFailedJob::GetMockHttpUrl(net::ERR_CONNECTION_FAILED));

  SendReport(&reporter, host_port, report_uri, base::Time(), ssl_info);

  // Wait to make sure the failure callback is called.
  run_loop.Run();
}

// Test that a sent report has the right format.
TEST_F(ExpectCTReporterTest, SendReport) {
  base::HistogramTester histograms;
  histograms.ExpectTotalCount(kFailureHistogramName, 0);
  histograms.ExpectTotalCount(kSendHistogramName, 0);

  TestCertificateReportSender* sender = new TestCertificateReportSender();
  net::TestURLRequestContext context;
  ExpectCTReporter reporter(&context, base::Closure(), base::Closure());
  reporter.report_sender_.reset(sender);
  EXPECT_TRUE(sender->latest_report_uri().is_empty());
  EXPECT_TRUE(sender->latest_serialized_report().empty());

  net::SSLInfo ssl_info;
  ssl_info.cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  ssl_info.unverified_cert = net::ImportCertFromFile(
      net::GetTestCertsDirectory(), "localhost_cert.pem");

  base::Time now = base::Time::Now();

  // Append a variety of SCTs: two of each possible status, with a
  // mixture of different origins.

  // The particular value of the log ID doesn't matter; it just has to be the
  // correct length.
  const unsigned char kTestLogId[] = {
      0xdf, 0x1c, 0x2e, 0xc1, 0x15, 0x00, 0x94, 0x52, 0x47, 0xa9, 0x61,
      0x68, 0x32, 0x5d, 0xdc, 0x5c, 0x79, 0x59, 0xe8, 0xf7, 0xc6, 0xd3,
      0x88, 0xfc, 0x00, 0x2e, 0x0b, 0xbd, 0x3f, 0x74, 0xd7, 0x01};
  const std::string log_id(reinterpret_cast<const char*>(kTestLogId),
                           sizeof(kTestLogId));
  // The values of the extensions and signature data don't matter
  // either. However, each SCT has to be unique for the test expectation to be
  // checked properly in CheckExpectCTReport(), so each SCT has a unique
  // extensions value to make sure the serialized SCTs are unique.
  MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                       log_id, "extensions1", "signature1", now,
                       net::ct::SCT_STATUS_LOG_UNKNOWN,
                       &ssl_info.signed_certificate_timestamps);
  MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                       log_id, "extensions2", "signature2", now,
                       net::ct::SCT_STATUS_LOG_UNKNOWN,
                       &ssl_info.signed_certificate_timestamps);

  MakeTestSCTAndStatus(
      net::ct::SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION, log_id,
      "extensions3", "signature1", now, net::ct::SCT_STATUS_INVALID_TIMESTAMP,
      &ssl_info.signed_certificate_timestamps);

  MakeTestSCTAndStatus(
      net::ct::SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION, log_id,
      "extensions4", "signature1", now, net::ct::SCT_STATUS_INVALID_SIGNATURE,
      &ssl_info.signed_certificate_timestamps);

  MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                       log_id, "extensions5", "signature2", now,
                       net::ct::SCT_STATUS_INVALID_SIGNATURE,
                       &ssl_info.signed_certificate_timestamps);

  MakeTestSCTAndStatus(
      net::ct::SignedCertificateTimestamp::SCT_FROM_OCSP_RESPONSE, log_id,
      "extensions6", "signature1", now, net::ct::SCT_STATUS_OK,
      &ssl_info.signed_certificate_timestamps);
  MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                       log_id, "extensions7", "signature2", now,
                       net::ct::SCT_STATUS_OK,
                       &ssl_info.signed_certificate_timestamps);

  const char kExpirationTimeStr[] = "2017-01-01T00:00:00.000Z";
  base::Time expiration;
  ASSERT_TRUE(
      base::Time::FromUTCExploded({2017, 1, 0, 1, 0, 0, 0, 0}, &expiration));

  const std::string report_path = "/report";
  base::RunLoop cors_run_loop;
  test_server().RegisterRequestHandler(base::Bind(&HandleReportPreflightForPath,
                                                  report_path, kGoodCorsHeaders,
                                                  cors_run_loop.QuitClosure()));
  ASSERT_TRUE(test_server().Start());
  const GURL report_uri = test_server().GetURL(report_path);

  // Check that the report is sent and contains the correct information.
  reporter.OnExpectCTFailed(net::HostPortPair::FromURL(report_uri), report_uri,
                            expiration, ssl_info.cert.get(),
                            ssl_info.unverified_cert.get(),
                            ssl_info.signed_certificate_timestamps);

  // A CORS preflight request should be sent before the actual report.
  cors_run_loop.Run();
  sender->WaitForReport(report_uri);

  EXPECT_EQ(report_uri, sender->latest_report_uri());
  EXPECT_FALSE(sender->latest_serialized_report().empty());
  EXPECT_EQ("application/expect-ct-report+json; charset=utf-8",
            sender->latest_content_type());
  ASSERT_NO_FATAL_FAILURE(CheckExpectCTReport(
      sender->latest_serialized_report(),
      net::HostPortPair::FromURL(report_uri), kExpirationTimeStr, ssl_info));

  histograms.ExpectTotalCount(kFailureHistogramName, 0);
  histograms.ExpectTotalCount(kSendHistogramName, 1);
  histograms.ExpectBucketCount(kSendHistogramName, true, 1);
}

// Test that the success callback is called when a report is successfully sent.
TEST_F(ExpectCTReporterTest, SendReportSuccessCallback) {
  test_server().RegisterRequestHandler(base::Bind(
      &HandleReportPreflight, kGoodCorsHeaders, base::RepeatingClosure()));
  // This test actually sends the report to the testserver, so register a
  // handler that will return OK.
  test_server().RegisterRequestHandler(base::Bind(&ReplyToPostWith200));
  ASSERT_TRUE(test_server().Start());

  base::RunLoop run_loop;

  net::TestURLRequestContext context;
  ExpectCTReporter reporter(&context, run_loop.QuitClosure(), base::Closure());

  net::SSLInfo ssl_info;
  ssl_info.cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  ssl_info.unverified_cert = net::ImportCertFromFile(
      net::GetTestCertsDirectory(), "localhost_cert.pem");

  // The particular value of the log ID doesn't matter; it just has to be the
  // correct length.
  const unsigned char kTestLogId[] = {
      0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
      0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
      0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01};
  const std::string log_id(reinterpret_cast<const char*>(kTestLogId),
                           sizeof(kTestLogId));
  MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                       log_id, "extensions1", "signature1", base::Time::Now(),
                       net::ct::SCT_STATUS_LOG_UNKNOWN,
                       &ssl_info.signed_certificate_timestamps);

  base::Time expiration;
  ASSERT_TRUE(
      base::Time::FromUTCExploded({2017, 1, 0, 1, 0, 0, 0, 0}, &expiration));

  const GURL report_uri = test_server().GetURL("/report");

  reporter.OnExpectCTFailed(net::HostPortPair::FromURL(report_uri), report_uri,
                            expiration, ssl_info.cert.get(),
                            ssl_info.unverified_cert.get(),
                            ssl_info.signed_certificate_timestamps);

  // Wait to check that the success callback is run.
  run_loop.Run();
}

// Test that report preflight responses can contain whitespace.
TEST_F(ExpectCTReporterTest, PreflightContainsWhitespace) {
  const std::string report_path = "/report";
  std::map<std::string, std::string> cors_headers = kGoodCorsHeaders;
  cors_headers["Access-Control-Allow-Methods"] = "GET, POST";
  base::RunLoop cors_run_loop;
  test_server().RegisterRequestHandler(base::Bind(&HandleReportPreflightForPath,
                                                  report_path, cors_headers,
                                                  cors_run_loop.QuitClosure()));
  ASSERT_TRUE(test_server().Start());

  TestCertificateReportSender* sender = new TestCertificateReportSender();
  net::TestURLRequestContext context;
  ExpectCTReporter reporter(&context, base::Closure(), base::Closure());
  reporter.report_sender_.reset(sender);
  EXPECT_TRUE(sender->latest_report_uri().is_empty());
  EXPECT_TRUE(sender->latest_serialized_report().empty());

  net::SSLInfo ssl_info;
  ssl_info.cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  ssl_info.unverified_cert = net::ImportCertFromFile(
      net::GetTestCertsDirectory(), "localhost_cert.pem");

  const GURL report_uri = test_server().GetURL(report_path);
  reporter.OnExpectCTFailed(net::HostPortPair::FromURL(report_uri), report_uri,
                            base::Time::Now(), ssl_info.cert.get(),
                            ssl_info.unverified_cert.get(),
                            ssl_info.signed_certificate_timestamps);

  // A CORS preflight request should be sent before the actual report.
  cors_run_loop.Run();
  sender->WaitForReport(report_uri);

  EXPECT_EQ(report_uri, sender->latest_report_uri());
  EXPECT_FALSE(sender->latest_serialized_report().empty());
}

// Test that no report is sent when the CORS preflight returns an invalid
// Access-Control-Allow-Origin.
TEST_F(ExpectCTReporterTest, BadCorsPreflightResponseOrigin) {
  TestCertificateReportSender* sender = new TestCertificateReportSender();
  net::TestURLRequestContext context;
  ExpectCTReporter reporter(&context, base::Closure(), base::Closure());
  reporter.report_sender_.reset(sender);
  EXPECT_TRUE(sender->latest_report_uri().is_empty());
  EXPECT_TRUE(sender->latest_serialized_report().empty());

  net::SSLInfo ssl_info;
  ssl_info.cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  ssl_info.unverified_cert = net::ImportCertFromFile(
      net::GetTestCertsDirectory(), "localhost_cert.pem");

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kExpectCTReporting);
  EXPECT_TRUE(sender->latest_serialized_report().empty());
  ASSERT_NO_FATAL_FAILURE(TestForReportPreflightFailure(
      &reporter, sender, net::HostPortPair("example.test", 443), ssl_info,
      "Access-Control-Allow-Origin", "https://another-origin.test", "null"));
}

// Test that no report is sent when the CORS preflight returns an invalid
// Access-Control-Allow-Methods.
TEST_F(ExpectCTReporterTest, BadCorsPreflightResponseMethods) {
  TestCertificateReportSender* sender = new TestCertificateReportSender();
  net::TestURLRequestContext context;
  ExpectCTReporter reporter(&context, base::Closure(), base::Closure());
  reporter.report_sender_.reset(sender);
  EXPECT_TRUE(sender->latest_report_uri().is_empty());
  EXPECT_TRUE(sender->latest_serialized_report().empty());

  net::SSLInfo ssl_info;
  ssl_info.cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  ssl_info.unverified_cert = net::ImportCertFromFile(
      net::GetTestCertsDirectory(), "localhost_cert.pem");

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kExpectCTReporting);
  EXPECT_TRUE(sender->latest_serialized_report().empty());
  ASSERT_NO_FATAL_FAILURE(TestForReportPreflightFailure(
      &reporter, sender, net::HostPortPair("example.test", 443), ssl_info,
      "Access-Control-Allow-Methods", "GET,HEAD,POSSSST", "POST"));
}

// Test that no report is sent when the CORS preflight returns an invalid
// Access-Control-Allow-Headers.
TEST_F(ExpectCTReporterTest, BadCorsPreflightResponseHeaders) {
  TestCertificateReportSender* sender = new TestCertificateReportSender();
  net::TestURLRequestContext context;
  ExpectCTReporter reporter(&context, base::Closure(), base::Closure());
  reporter.report_sender_.reset(sender);
  EXPECT_TRUE(sender->latest_report_uri().is_empty());
  EXPECT_TRUE(sender->latest_serialized_report().empty());

  net::SSLInfo ssl_info;
  ssl_info.cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  ssl_info.unverified_cert = net::ImportCertFromFile(
      net::GetTestCertsDirectory(), "localhost_cert.pem");

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kExpectCTReporting);
  EXPECT_TRUE(sender->latest_serialized_report().empty());
  ASSERT_NO_FATAL_FAILURE(TestForReportPreflightFailure(
      &reporter, sender, net::HostPortPair("example.test", 443), ssl_info,
      "Access-Control-Allow-Headers", "Not-Content-Type", "Content-Type"));
}

}  // namespace network
