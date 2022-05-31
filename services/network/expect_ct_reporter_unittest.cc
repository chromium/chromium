// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/expect_ct_reporter.h"

#include <memory>
#include <string>

#include "base/base64.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/base/network_isolation_key.h"
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
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/public/cpp/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace network {
namespace {

// A test ReportSender that exposes the latest report URI and
// serialized report to be sent.
class TestCertificateReportSender : public net::ReportSender {
 public:
  TestCertificateReportSender()
      : ReportSender(nullptr, TRAFFIC_ANNOTATION_FOR_TESTS) {}
  ~TestCertificateReportSender() override {}

  void Send(
      const GURL& report_uri,
      base::StringPiece content_type,
      base::StringPiece serialized_report,
      const net::NetworkIsolationKey& network_isolation_key,
      base::OnceCallback<void()> success_callback,
      base::OnceCallback<void(const GURL&, int, int)> error_callback) override {
    sent_report_count_++;
    latest_report_uri_ = report_uri;
    latest_serialized_report_.assign(serialized_report.data(),
                                     serialized_report.size());
    latest_content_type_.assign(content_type.data(), content_type.size());
    latest_network_isolation_key_ = network_isolation_key;
    if (!report_callback_.is_null()) {
      EXPECT_EQ(expected_report_uri_, latest_report_uri_);
      std::move(report_callback_).Run();
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

  const net::NetworkIsolationKey latest_network_isolation_key() const {
    return latest_network_isolation_key_;
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
  net::NetworkIsolationKey latest_network_isolation_key_;
  base::OnceClosure report_callback_;
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
    const base::Value::List& chain_list) {
  std::vector<std::string> pem_encoded_chain;
  expected_cert->GetPEMEncodedChain(&pem_encoded_chain);
  ASSERT_EQ(pem_encoded_chain.size(), chain_list.size());

  for (size_t i = 0; i < pem_encoded_chain.size(); i++) {
    ASSERT_TRUE(chain_list[i].is_string());
    EXPECT_EQ(pem_encoded_chain[i], chain_list[i].GetString());
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
    const base::Value::List& report_list) {
  std::string expected_serialized_sct;
  if (!net::ct::EncodeSignedCertificateTimestamp(expected_sct,
                                                 &expected_serialized_sct)) {
    return ::testing::AssertionFailure() << "Failed to serialize SCT";
  }

  for (const base::Value& report_sct_value : report_list) {
    if (!report_sct_value.is_dict()) {
      return ::testing::AssertionFailure()
             << "Failed to get dictionary value from report SCT list";
    }
    const base::Value::Dict& report_sct = report_sct_value.GetDict();
    const std::string* serialized_sct = report_sct.FindString("serialized_sct");
    EXPECT_TRUE(serialized_sct);
    std::string decoded_serialized_sct;
    EXPECT_TRUE(base::Base64Decode(*serialized_sct, &decoded_serialized_sct));
    if (decoded_serialized_sct != expected_serialized_sct)
      continue;

    const std::string* source = report_sct.FindString("source");
    EXPECT_TRUE(source);
    EXPECT_EQ(expected_sct->origin, SCTOriginStringToOrigin(*source));

    const std::string* report_status = report_sct.FindString("status");
    EXPECT_TRUE(report_status);
    switch (expected_status) {
      case net::ct::SCT_STATUS_LOG_UNKNOWN:
        EXPECT_EQ("unknown", *report_status);
        break;
      case net::ct::SCT_STATUS_INVALID_SIGNATURE:
      case net::ct::SCT_STATUS_INVALID_TIMESTAMP: {
        EXPECT_EQ("invalid", *report_status);
        break;
      }
      case net::ct::SCT_STATUS_OK: {
        EXPECT_EQ("valid", *report_status);
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
    const base::Value::List& scts) {
  EXPECT_EQ(expected_scts.size(), scts.size());
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
  absl::optional<base::Value> value(base::JSONReader::Read(serialized_report));
  ASSERT_TRUE(value);

  base::Value::Dict* outer_report_dict = value->GetIfDict();
  ASSERT_TRUE(outer_report_dict);

  base::Value::Dict* report_dict =
      outer_report_dict->FindDict("expect-ct-report");
  ASSERT_TRUE(report_dict);

  std::string* report_hostname = report_dict->FindString("hostname");
  EXPECT_TRUE(report_hostname);
  EXPECT_EQ(host_port.host(), *report_hostname);
  absl::optional<int> report_port = report_dict->FindInt("port");
  EXPECT_EQ(host_port.port(), report_port);

  std::string* report_expiration =
      report_dict->FindString("effective-expiration-date");
  EXPECT_TRUE(report_expiration);
  EXPECT_EQ(expiration, *report_expiration);

  const base::Value::List* report_served_certificate_chain =
      report_dict->FindList("served-certificate-chain");
  ASSERT_TRUE(report_served_certificate_chain);
  ASSERT_NO_FATAL_FAILURE(CheckReportCertificateChain(
      ssl_info.unverified_cert, *report_served_certificate_chain));

  const base::Value::List* report_validated_certificate_chain =
      report_dict->FindList("validated-certificate-chain");
  ASSERT_TRUE(report_validated_certificate_chain);
  ASSERT_NO_FATAL_FAILURE(CheckReportCertificateChain(
      ssl_info.cert, *report_validated_certificate_chain));

  const base::Value::List* report_scts = report_dict->FindList("scts");
  ASSERT_TRUE(report_scts);

  ASSERT_NO_FATAL_FAILURE(
      CheckReportSCTs(ssl_info.signed_certificate_timestamps, *report_scts));
}

// A test network delegate that allows the user to specify a callback to
// be run whenever a net::URLRequest is destroyed.
class TestExpectCTNetworkDelegate : public net::NetworkDelegateImpl {
 public:
  TestExpectCTNetworkDelegate() = default;

  TestExpectCTNetworkDelegate(const TestExpectCTNetworkDelegate&) = delete;
  TestExpectCTNetworkDelegate& operator=(const TestExpectCTNetworkDelegate&) =
      delete;

  using OnBeforeURLRequestCallback =
      base::RepeatingCallback<void(net::URLRequest* request)>;

  void set_on_before_url_request_callback(
      const OnBeforeURLRequestCallback& on_before_url_request_callback) {
    on_before_url_request_callback_ = on_before_url_request_callback;
  }

  void set_url_request_destroyed_callback(
      const base::RepeatingClosure& callback) {
    url_request_destroyed_callback_ = callback;
  }

  // net::NetworkDelegateImpl:
  int OnBeforeURLRequest(net::URLRequest* request,
                         net::CompletionOnceCallback callback,
                         GURL* new_url) override {
    if (on_before_url_request_callback_)
      on_before_url_request_callback_.Run(request);
    return net::OK;
  }
  void OnURLRequestDestroyed(net::URLRequest* request) override {
    if (url_request_destroyed_callback_)
      url_request_destroyed_callback_.Run();
  }

 private:
  OnBeforeURLRequestCallback on_before_url_request_callback_;
  base::RepeatingClosure url_request_destroyed_callback_;
};

// A test fixture that allows tests to send a report and wait until the
// net::URLRequest that sent the report is destroyed.
class ExpectCTReporterWaitTest : public ::testing::Test {
 public:
  ExpectCTReporterWaitTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}

  ExpectCTReporterWaitTest(const ExpectCTReporterWaitTest&) = delete;
  ExpectCTReporterWaitTest& operator=(const ExpectCTReporterWaitTest&) = delete;

  void SetUp() override {
    auto context_builder = net::CreateTestURLRequestContextBuilder();
    context_builder->set_network_delegate(
        std::make_unique<TestExpectCTNetworkDelegate>());

    context_ = context_builder->Build();
    net::URLRequestFailedJob::AddUrlHandler();
  }

  void TearDown() override {
    net::URLRequestFilter::GetInstance()->ClearHandlers();
  }

  net::URLRequestContext* context() { return context_.get(); }
  TestExpectCTNetworkDelegate* network_delegate() {
    // This is safe as we provided a TestExpectCTNetworkDelegate in SetUp().
    return static_cast<TestExpectCTNetworkDelegate*>(
        context_->network_delegate());
  }

 protected:
  void SendReport(ExpectCTReporter* reporter,
                  const net::HostPortPair& host_port,
                  const GURL& report_uri,
                  base::Time expiration,
                  const net::SSLInfo& ssl_info) {
    base::RunLoop run_loop;
    network_delegate()->set_url_request_destroyed_callback(
        run_loop.QuitClosure());
    reporter->OnExpectCTFailed(
        host_port, report_uri, expiration, ssl_info.cert.get(),
        ssl_info.unverified_cert.get(), ssl_info.signed_certificate_timestamps,
        net::NetworkIsolationKey());
    run_loop.Run();
  }

 private:
  std::unique_ptr<net::URLRequestContext> context_;
  base::test::TaskEnvironment task_environment_;
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

  // Tests that reports are sent when the CORS preflight request returns the
  // header field |preflight_header_name| with value given by
  // |preflight_header_good_value|.
  void TestForReportPreflightSuccess(
      ExpectCTReporter* reporter,
      TestCertificateReportSender* sender,
      const net::SSLInfo& ssl_info,
      const std::string& preflight_header_name,
      const std::string& preflight_header_good_value) {
    const std::string report_path = "/report";
    std::map<std::string, std::string> cors_headers = kGoodCorsHeaders;
    if (preflight_header_good_value.empty()) {
      cors_headers.erase(preflight_header_name);
    } else {
      cors_headers[preflight_header_name] = preflight_header_good_value;
    }
    base::RunLoop cors_run_loop;
    test_server().RegisterRequestHandler(
        base::BindRepeating(&HandleReportPreflightForPath, report_path,
                            cors_headers, cors_run_loop.QuitClosure()));
    ASSERT_TRUE(test_server().Start());
    const GURL report_uri = test_server().GetURL(report_path);
    reporter->OnExpectCTFailed(
        net::HostPortPair::FromURL(report_uri), report_uri, base::Time::Now(),
        ssl_info.cert.get(), ssl_info.unverified_cert.get(),
        ssl_info.signed_certificate_timestamps, net::NetworkIsolationKey());
    // A CORS preflight request should be sent before the actual report.
    cors_run_loop.Run();
    sender->WaitForReport(report_uri);

    EXPECT_EQ(report_uri, sender->latest_report_uri());
    EXPECT_FALSE(sender->latest_serialized_report().empty());
  }

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
        base::BindRepeating(&HandleReportPreflightForPath, fail_path,
                            bad_cors_headers, bad_cors_run_loop.QuitClosure()));
    report_server_.RegisterRequestHandler(
        base::BindRepeating(&HandleReportPreflightForPath, successful_path,
                            good_cors_headers, base::RepeatingClosure()));
    ASSERT_TRUE(report_server_.Start());

    const GURL fail_report_uri = test_server().GetURL(fail_path);
    const GURL successful_report_uri = test_server().GetURL(successful_path);
    const net::NetworkIsolationKey network_isolation_key =
        net::NetworkIsolationKey::CreateTransient();

    reporter->OnExpectCTFailed(
        host_port, fail_report_uri, base::Time(), ssl_info.cert.get(),
        ssl_info.unverified_cert.get(), ssl_info.signed_certificate_timestamps,
        network_isolation_key);
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
        ssl_info.unverified_cert.get(), ssl_info.signed_certificate_timestamps,
        network_isolation_key);
    sender->WaitForReport(successful_report_uri);
    EXPECT_EQ(successful_report_uri, sender->latest_report_uri());
    EXPECT_EQ(network_isolation_key, sender->latest_network_isolation_key());
    EXPECT_EQ(1, sender->sent_report_count());
  }

 private:
  base::test::TaskEnvironment task_environment_;
  net::EmbeddedTestServer report_server_;
};

}  // namespace

// Test that no report is sent when the feature is not enabled.
TEST_F(ExpectCTReporterTest, FeatureDisabled) {
  test_server().RegisterRequestHandler(base::BindRepeating(
      &HandleReportPreflight, kGoodCorsHeaders, base::RepeatingClosure()));
  ASSERT_TRUE(test_server().Start());

  TestCertificateReportSender* sender = new TestCertificateReportSender();
  auto context_builder = net::CreateTestURLRequestContextBuilder();
  auto context = context_builder->Build();
  ExpectCTReporter reporter(context.get(), base::NullCallback(),
                            base::NullCallback());
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
        ssl_info.unverified_cert.get(), ssl_info.signed_certificate_timestamps,
        net::NetworkIsolationKey());
    EXPECT_TRUE(sender->latest_report_uri().is_empty());
    EXPECT_TRUE(sender->latest_serialized_report().empty());
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
        ssl_info.unverified_cert.get(), ssl_info.signed_certificate_timestamps,
        net::NetworkIsolationKey());
    sender->WaitForReport(report_uri);
    EXPECT_EQ(report_uri, sender->latest_report_uri());
    EXPECT_EQ(1, sender->sent_report_count());
  }
}

// Test that no report is sent if the report URI is empty.
TEST_F(ExpectCTReporterTest, EmptyReportURI) {
  TestCertificateReportSender* sender = new TestCertificateReportSender();
  auto context_builder = net::CreateTestURLRequestContextBuilder();
  auto context = context_builder->Build();
  ExpectCTReporter reporter(context.get(), base::NullCallback(),
                            base::NullCallback());
  reporter.report_sender_.reset(sender);
  EXPECT_TRUE(sender->latest_report_uri().is_empty());
  EXPECT_TRUE(sender->latest_serialized_report().empty());

  reporter.OnExpectCTFailed(net::HostPortPair(), GURL(), base::Time(), nullptr,
                            nullptr,
                            net::SignedCertificateTimestampAndStatusList(),
                            net::NetworkIsolationKey());
  EXPECT_TRUE(sender->latest_report_uri().is_empty());
  EXPECT_TRUE(sender->latest_serialized_report().empty());
}

// Test that if a report fails to send, the failure callback is called.
TEST_F(ExpectCTReporterWaitTest, SendReportFailureCallback) {
  base::RunLoop run_loop;
  ExpectCTReporter reporter(context(), base::NullCallback(),
                            run_loop.QuitClosure());

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
  TestCertificateReportSender* sender = new TestCertificateReportSender();
  auto context_builder = net::CreateTestURLRequestContextBuilder();
  auto context = context_builder->Build();
  ExpectCTReporter reporter(context.get(), base::NullCallback(),
                            base::NullCallback());
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
  test_server().RegisterRequestHandler(
      base::BindRepeating(&HandleReportPreflightForPath, report_path,
                          kGoodCorsHeaders, cors_run_loop.QuitClosure()));
  ASSERT_TRUE(test_server().Start());
  const GURL report_uri = test_server().GetURL(report_path);

  // Check that the report is sent and contains the correct information.
  reporter.OnExpectCTFailed(
      net::HostPortPair::FromURL(report_uri), report_uri, expiration,
      ssl_info.cert.get(), ssl_info.unverified_cert.get(),
      ssl_info.signed_certificate_timestamps, net::NetworkIsolationKey());

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
}

// Test that the success callback is called when a report is successfully sent.
TEST_F(ExpectCTReporterTest, SendReportSuccessCallback) {
  test_server().RegisterRequestHandler(base::BindRepeating(
      &HandleReportPreflight, kGoodCorsHeaders, base::RepeatingClosure()));
  // This test actually sends the report to the testserver, so register a
  // handler that will return OK.
  test_server().RegisterRequestHandler(
      base::BindRepeating(&ReplyToPostWith200));
  ASSERT_TRUE(test_server().Start());

  base::RunLoop run_loop;

  auto context_builder = net::CreateTestURLRequestContextBuilder();
  auto context = context_builder->Build();
  ExpectCTReporter reporter(context.get(), run_loop.QuitClosure(),
                            base::NullCallback());

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

  reporter.OnExpectCTFailed(
      net::HostPortPair::FromURL(report_uri), report_uri, expiration,
      ssl_info.cert.get(), ssl_info.unverified_cert.get(),
      ssl_info.signed_certificate_timestamps, net::NetworkIsolationKey());

  // Wait to check that the success callback is run.
  run_loop.Run();
}

// Test that report preflight requests use the correct NetworkIsolationKey.
TEST_F(ExpectCTReporterTest, PreflightUsesNetworkIsolationKey) {
  net::NetworkIsolationKey network_isolation_key =
      net::NetworkIsolationKey::CreateTransient();

  const std::string report_path = "/report";
  std::map<std::string, std::string> cors_headers = kGoodCorsHeaders;
  base::RunLoop cors_run_loop;
  test_server().RegisterRequestHandler(
      base::BindRepeating(&HandleReportPreflightForPath, report_path,
                          cors_headers, cors_run_loop.QuitClosure()));
  ASSERT_TRUE(test_server().Start());

  TestCertificateReportSender* sender = new TestCertificateReportSender();

  auto context_builder = net::CreateTestURLRequestContextBuilder();
  auto& network_delegate = *context_builder->set_network_delegate(
      std::make_unique<TestExpectCTNetworkDelegate>());
  auto context = context_builder->Build();

  ExpectCTReporter reporter(context.get(), base::NullCallback(),
                            base::NullCallback());
  reporter.report_sender_.reset(sender);
  EXPECT_TRUE(sender->latest_report_uri().is_empty());
  EXPECT_TRUE(sender->latest_serialized_report().empty());

  net::SSLInfo ssl_info;
  ssl_info.cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  ssl_info.unverified_cert = net::ImportCertFromFile(
      net::GetTestCertsDirectory(), "localhost_cert.pem");

  base::RunLoop before_url_request_run_loop;
  network_delegate.set_on_before_url_request_callback(
      base::BindLambdaForTesting([&](net::URLRequest* request) {
        EXPECT_EQ(network_isolation_key,
                  request->isolation_info().network_isolation_key());
        before_url_request_run_loop.Quit();
      }));

  const GURL report_uri = test_server().GetURL(report_path);
  reporter.OnExpectCTFailed(
      net::HostPortPair::FromURL(report_uri), report_uri, base::Time::Now(),
      ssl_info.cert.get(), ssl_info.unverified_cert.get(),
      ssl_info.signed_certificate_timestamps, network_isolation_key);

  // Make sure the OnBeforeURLRequestCallback is hit.
  before_url_request_run_loop.Run();

  // A CORS preflight request should be sent before the actual report.
  cors_run_loop.Run();
  sender->WaitForReport(report_uri);

  EXPECT_EQ(report_uri, sender->latest_report_uri());
  EXPECT_FALSE(sender->latest_serialized_report().empty());
}

// Test that report preflight responses can contain whitespace.
TEST_F(ExpectCTReporterTest, PreflightContainsWhitespace) {
  TestCertificateReportSender* sender = new TestCertificateReportSender();
  auto context_builder = net::CreateTestURLRequestContextBuilder();
  auto context = context_builder->Build();
  ExpectCTReporter reporter(context.get(), base::NullCallback(),
                            base::NullCallback());
  reporter.report_sender_.reset(sender);
  EXPECT_TRUE(sender->latest_report_uri().is_empty());
  EXPECT_TRUE(sender->latest_serialized_report().empty());

  net::SSLInfo ssl_info;
  ssl_info.cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  ssl_info.unverified_cert = net::ImportCertFromFile(
      net::GetTestCertsDirectory(), "localhost_cert.pem");

  ASSERT_NO_FATAL_FAILURE(TestForReportPreflightSuccess(
      &reporter, sender, ssl_info, "Access-Control-Allow-Methods",
      "GET, POST"));
}

// Test that report preflight responses can contain
// "Access-Control-Allow-Methods: *"
TEST_F(ExpectCTReporterTest, PreflightMethodsContainsWildcard) {
  TestCertificateReportSender* sender = new TestCertificateReportSender();
  auto context_builder = net::CreateTestURLRequestContextBuilder();
  auto context = context_builder->Build();
  ExpectCTReporter reporter(context.get(), base::NullCallback(),
                            base::NullCallback());
  reporter.report_sender_.reset(sender);
  EXPECT_TRUE(sender->latest_report_uri().is_empty());
  EXPECT_TRUE(sender->latest_serialized_report().empty());

  net::SSLInfo ssl_info;
  ssl_info.cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  ssl_info.unverified_cert = net::ImportCertFromFile(
      net::GetTestCertsDirectory(), "localhost_cert.pem");

  ASSERT_NO_FATAL_FAILURE(TestForReportPreflightSuccess(
      &reporter, sender, ssl_info, "Access-Control-Allow-Methods", "*"));
}

// Test that report preflight responses can contain
// "Access-Control-Allow-Headers: *"
TEST_F(ExpectCTReporterTest, PreflightHeadersContainsWildcard) {
  TestCertificateReportSender* sender = new TestCertificateReportSender();
  auto context_builder = net::CreateTestURLRequestContextBuilder();
  auto context = context_builder->Build();
  ExpectCTReporter reporter(context.get(), base::NullCallback(),
                            base::NullCallback());
  reporter.report_sender_.reset(sender);
  EXPECT_TRUE(sender->latest_report_uri().is_empty());
  EXPECT_TRUE(sender->latest_serialized_report().empty());

  net::SSLInfo ssl_info;
  ssl_info.cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  ssl_info.unverified_cert = net::ImportCertFromFile(
      net::GetTestCertsDirectory(), "localhost_cert.pem");

  ASSERT_NO_FATAL_FAILURE(TestForReportPreflightSuccess(
      &reporter, sender, ssl_info, "Access-Control-Allow-Headers", "*"));
}

// Test that no report is sent when the CORS preflight returns an invalid
// Access-Control-Allow-Origin.
TEST_F(ExpectCTReporterTest, BadCorsPreflightResponseOrigin) {
  TestCertificateReportSender* sender = new TestCertificateReportSender();
  auto context_builder = net::CreateTestURLRequestContextBuilder();
  auto context = context_builder->Build();
  ExpectCTReporter reporter(context.get(), base::NullCallback(),
                            base::NullCallback());
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

// Test that report is sent when the CORS preflight does not include an
// Access-Control-Allow-Methods header.
TEST_F(ExpectCTReporterTest, CorsPreflightWithNoAllowMethods) {
  TestCertificateReportSender* sender = new TestCertificateReportSender();
  auto context_builder = net::CreateTestURLRequestContextBuilder();
  auto context = context_builder->Build();
  ExpectCTReporter reporter(context.get(), base::NullCallback(),
                            base::NullCallback());
  reporter.report_sender_.reset(sender);
  EXPECT_TRUE(sender->latest_report_uri().is_empty());
  EXPECT_TRUE(sender->latest_serialized_report().empty());

  net::SSLInfo ssl_info;
  ssl_info.cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  ssl_info.unverified_cert = net::ImportCertFromFile(
      net::GetTestCertsDirectory(), "localhost_cert.pem");

  ASSERT_NO_FATAL_FAILURE(TestForReportPreflightSuccess(
      &reporter, sender, ssl_info, "Access-Control-Allow-Methods", ""));
}

// Test that report is sent when the CORS preflight returns an invalid
// Access-Control-Allow-Methods.
TEST_F(ExpectCTReporterTest, BadCorsPreflightResponseMethods) {
  TestCertificateReportSender* sender = new TestCertificateReportSender();
  auto context_builder = net::CreateTestURLRequestContextBuilder();
  auto context = context_builder->Build();
  ExpectCTReporter reporter(context.get(), base::NullCallback(),
                            base::NullCallback());
  reporter.report_sender_.reset(sender);
  EXPECT_TRUE(sender->latest_report_uri().is_empty());
  EXPECT_TRUE(sender->latest_serialized_report().empty());

  net::SSLInfo ssl_info;
  ssl_info.cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  ssl_info.unverified_cert = net::ImportCertFromFile(
      net::GetTestCertsDirectory(), "localhost_cert.pem");

  ASSERT_NO_FATAL_FAILURE(TestForReportPreflightSuccess(
      &reporter, sender, ssl_info, "Access-Control-Allow-Methods",
      "GET,HEAD,POSSSST"));
}

// Test that no report is sent when the CORS preflight returns an invalid
// Access-Control-Allow-Headers.
TEST_F(ExpectCTReporterTest, BadCorsPreflightResponseHeaders) {
  TestCertificateReportSender* sender = new TestCertificateReportSender();
  auto context_builder = net::CreateTestURLRequestContextBuilder();
  auto context = context_builder->Build();
  ExpectCTReporter reporter(context.get(), base::NullCallback(),
                            base::NullCallback());
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
