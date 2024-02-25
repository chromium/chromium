// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/sct_auditing/sct_auditing_reporter.h"

#include "base/base64.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "net/base/hash_value.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/proto/sct_audit_report.pb.h"
#include "services/network/test/fake_test_cert_verifier_params_factory.h"
#include "services/network/test/test_network_context_client.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/utility/utility.h"

namespace network {

namespace {

constexpr char kLeafHashBase64[] = "bGVhZl9oYXNo";
// 42 seconds after the UNIX epoch.
constexpr char kIssuedSerialized[] = "11644473642000000";
constexpr char kLogIdBase64[] = "bG9nX2lk";
// 42 seconds.
constexpr char kLogMMDSerialized[] = "42000000";
// 10 seconds after the UNIX epoch.
constexpr char kCertExpirySerialized[] = "11644473610000000";

constexpr char kTestReportURL[] = "https://report.com/";
constexpr char kTestLookupURL[] = "https://lookup-uri.com/length/$1/prefix/$2/";
constexpr char kTestLookupDomain[] = "lookup-uri.com";

constexpr base::TimeDelta kExpectedIngestionDelay = base::Hours(1);
constexpr base::TimeDelta kMaxIngestionRandomDelay = base::TimeDelta();

std::string ExtractRESTURLParameter(std::string url, std::string param) {
  size_t length_start = url.find(param) + param.size() + 1;
  size_t length_end = url.find('/', length_start);
  return url.substr(length_start, length_end - length_start);
}

}  // namespace

class SCTAuditingReporterTest : public testing::Test {
 protected:
  struct Response {
    std::string status = "OK";
    std::string hash_suffix = "some-arbitrary-sct";
    std::string log_id = "log_id";
    base::Time ingested_until;
    base::Time now;
  };

  SCTAuditingReporterTest()
      : network_service_(NetworkService::CreateForTesting()) {
    SCTAuditingReporter::SetRetryDelayForTesting(base::TimeDelta());

    reporter_metadata_.issued = base::Time::Now() - base::Days(30);
    reporter_metadata_.certificate_expiry = base::Time::Now() + base::Days(30);
    reporter_metadata_.log_id = "log_id";
    bool ok =
        base::HexStringToString("112233445566", &reporter_metadata_.leaf_hash);
    CHECK(ok);

    response_.ingested_until = base::Time::Now();
    response_.now = base::Time::Now();

    // Set up a NetworkContext.
    mojom::NetworkContextParamsPtr context_params =
        CreateNetworkContextParamsForTesting();
    context_params->cert_verifier_params =
        FakeTestCertVerifierParamsFactory::GetCertVerifierParams();
    context_params->sct_auditing_mode =
        mojom::SCTAuditingMode::kEnhancedSafeBrowsingReporting;
    network_context_ = std::make_unique<NetworkContext>(
        network_service_.get(),
        network_context_remote_.BindNewPipeAndPassReceiver(),
        std::move(context_params));

    // A NetworkContextClient is needed for querying/updating the report count.
    mojo::PendingRemote<network::mojom::NetworkContextClient>
        network_context_client_remote;
    network_context_client_ = std::make_unique<TestNetworkContextClient>(
        network_context_client_remote.InitWithNewPipeAndPassReceiver());
    network_context_->SetClient(std::move(network_context_client_remote));
  }

  // Creates a reporter.
  // By default, this function sets:
  //   * The SCT issued date is set to 30 days ago.
  //   * The certificate expiry is set for the next 30 days.
  //   * The log ID "log_id".
  //   * The leaf hash 0x112233445566.
  // This allows easily making a reporter that will immediately report an SCT
  // not returned by the lookup query. The parameters can be configured by
  // modifying the template |reporter_metadata_| object.
  SCTAuditingReporter MakeReporter() {
    auto report = std::make_unique<sct_auditing::SCTClientReport>();
    // Clone the metadata.
    SCTAuditingReporter::SCTHashdanceMetadata metadata =
        *SCTAuditingReporter::SCTHashdanceMetadata::FromValue(
            reporter_metadata_.ToValue());
    mojom::SCTAuditingConfigurationPtr configuration(std::in_place);
    configuration->log_expected_ingestion_delay = kExpectedIngestionDelay;
    configuration->log_max_ingestion_random_delay = kMaxIngestionRandomDelay;
    configuration->report_uri = GURL(kTestReportURL);
    configuration->hashdance_lookup_uri = GURL(kTestLookupURL);
    configuration->hashdance_traffic_annotation =
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
    configuration->traffic_annotation =
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
    return SCTAuditingReporter(
        network_context_.get(), net::HashValue(), std::move(report),
        /*is_hashdance=*/true, std::move(metadata), std::move(configuration),
        &url_loader_factory_, base::DoNothing(), base::DoNothing(),
        /*backoff_entry=*/nullptr);
  }

  // Simulates a response for a pending request with the values from the
  // |response_| template object.
  void SimulateResponse() {
    std::string leaf_hash_base64 = base::Base64Encode(response_.hash_suffix);
    std::string log_id_base64 = base::Base64Encode(response_.log_id);
    url_loader_factory_.SimulateResponseForPendingRequest(
        url_loader_factory_.GetPendingRequest(0)->request.url.spec(),
        base::ReplaceStringPlaceholders(
            R"(
          {
            "responseStatus": "$1",
            "hashSuffix": [
              "$2"
            ],
            "logStatus": [{
              "logId": "$3",
              "ingestedUntil": "$4"
            }],
            "now": "$5"
          }
        )",
            {
                response_.status,
                leaf_hash_base64,
                log_id_base64,
                base::TimeFormatAsIso8601(response_.ingested_until),
                base::TimeFormatAsIso8601(response_.now),
            },
            nullptr));
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME,
  };

  std::unique_ptr<NetworkService> network_service_;
  std::unique_ptr<NetworkContext> network_context_;
  std::unique_ptr<TestNetworkContextClient> network_context_client_;
  TestURLLoaderFactory url_loader_factory_;
  base::HistogramTester histograms;

  // Metadata used when creating a repoter.
  SCTAuditingReporter::SCTHashdanceMetadata reporter_metadata_;

  Response response_;

  // Stores the mojo::Remote<mojom::NetworkContext> of the most recently created
  // NetworkContext.
  mojo::Remote<mojom::NetworkContext> network_context_remote_;
};

TEST_F(SCTAuditingReporterTest, SCTHashdanceMetadataFromValue) {
  auto valid_value_dict = base::Value::Dict()
                              .Set("leaf_hash", kLeafHashBase64)
                              .Set("issued", kIssuedSerialized)
                              .Set("log_id", kLogIdBase64)
                              .Set("log_mmd", kLogMMDSerialized)
                              .Set("cert_expiry", kCertExpirySerialized);
  base::Value valid_value(std::move(valid_value_dict));

  auto metadata =
      SCTAuditingReporter::SCTHashdanceMetadata::FromValue(valid_value);
  ASSERT_TRUE(metadata);
  EXPECT_EQ(metadata->leaf_hash, "leaf_hash");
  EXPECT_EQ(metadata->issued, base::Time::UnixEpoch() + base::Seconds(42));
  EXPECT_EQ(metadata->log_id, "log_id");
  EXPECT_EQ(metadata->log_mmd, base::Seconds(42));
  EXPECT_EQ(metadata->certificate_expiry,
            base::Time::UnixEpoch() + base::Seconds(10));
  {
    base::Value invalid_value = valid_value.Clone();
    invalid_value.GetDict().Remove("leaf_hash");
    EXPECT_FALSE(
        SCTAuditingReporter::SCTHashdanceMetadata::FromValue(invalid_value));
  }
  {
    base::Value invalid_value = valid_value.Clone();
    invalid_value.GetDict().Remove("issued");
    EXPECT_FALSE(
        SCTAuditingReporter::SCTHashdanceMetadata::FromValue(invalid_value));
  }
  {
    base::Value invalid_value = valid_value.Clone();
    invalid_value.GetDict().Remove("log_id");
    EXPECT_FALSE(
        SCTAuditingReporter::SCTHashdanceMetadata::FromValue(invalid_value));
  }
  {
    base::Value invalid_value = valid_value.Clone();
    invalid_value.GetDict().Remove("log_mmd");
    EXPECT_FALSE(
        SCTAuditingReporter::SCTHashdanceMetadata::FromValue(invalid_value));
  }
  {
    base::Value invalid_value = valid_value.Clone();
    invalid_value.GetDict().Set("leaf_hash", "invalid base64");
    EXPECT_FALSE(
        SCTAuditingReporter::SCTHashdanceMetadata::FromValue(invalid_value));
  }
  {
    base::Value invalid_value = valid_value.Clone();
    invalid_value.GetDict().Set("log_id", "invalid base64");
    EXPECT_FALSE(
        SCTAuditingReporter::SCTHashdanceMetadata::FromValue(invalid_value));
  }
  {
    base::Value invalid_value = valid_value.Clone();
    invalid_value.GetDict().Remove("cert_expiry");
    EXPECT_FALSE(
        SCTAuditingReporter::SCTHashdanceMetadata::FromValue(invalid_value));
  }
  {
    EXPECT_FALSE(SCTAuditingReporter::SCTHashdanceMetadata::FromValue(
        base::Value("not a dict")));
  }
}

TEST_F(SCTAuditingReporterTest, SCTHashdanceMetadataToValue) {
  SCTAuditingReporter::SCTHashdanceMetadata metadata;
  metadata.leaf_hash = "leaf_hash";
  metadata.issued = base::Time::UnixEpoch() + base::Seconds(42);
  metadata.log_id = "log_id";
  metadata.log_mmd = base::Seconds(42);
  metadata.certificate_expiry = base::Time::UnixEpoch() + base::Seconds(10);
  base::Value value = metadata.ToValue();
  const base::Value::Dict* dict = value.GetIfDict();
  ASSERT_TRUE(dict);
  EXPECT_EQ(*dict->FindString("leaf_hash"), kLeafHashBase64);
  EXPECT_EQ(*dict->FindString("issued"), kIssuedSerialized);
  EXPECT_EQ(*dict->FindString("log_id"), kLogIdBase64);
  EXPECT_EQ(*dict->FindString("log_mmd"), kLogMMDSerialized);
  EXPECT_EQ(*dict->FindString("cert_expiry"), kCertExpirySerialized);
}

// Tests that a hashdance lookup that does not find the SCT reports it.
TEST_F(SCTAuditingReporterTest, HashdanceLookupNotFound) {
  SCTAuditingReporter reporter = MakeReporter();
  reporter.Start();

  EXPECT_EQ(url_loader_factory_.NumPending(), 1);
  TestURLLoaderFactory::PendingRequest* pending_request =
      url_loader_factory_.GetPendingRequest(0);
  EXPECT_TRUE(pending_request->request.url.DomainIs(kTestLookupDomain));
  std::string length =
      ExtractRESTURLParameter(pending_request->request.url.spec(), "length");
  EXPECT_EQ(length, "20");
  std::string prefix =
      ExtractRESTURLParameter(pending_request->request.url.spec(), "prefix");
  EXPECT_EQ(prefix, "112230");

  // Respond to the lookup request.
  SimulateResponse();

  // SCT should be reported.
  EXPECT_EQ(url_loader_factory_.NumPending(), 1);
  pending_request = url_loader_factory_.GetPendingRequest(0);
  EXPECT_EQ(pending_request->request.url.spec(), kTestReportURL);
  histograms.ExpectUniqueSample(
      "Security.SCTAuditing.OptOut.LookupQueryResult",
      SCTAuditingReporter::LookupQueryResult::kSCTSuffixNotFound, 1);
}

// Tests that a hashdance lookup that finds the SCT does not report it.
TEST_F(SCTAuditingReporterTest, HashdanceLookupFound) {
  SCTAuditingReporter reporter = MakeReporter();
  reporter.Start();

  EXPECT_EQ(url_loader_factory_.NumPending(), 1);
  TestURLLoaderFactory::PendingRequest* pending_request =
      url_loader_factory_.GetPendingRequest(0);
  EXPECT_TRUE(pending_request->request.url.DomainIs(kTestLookupDomain));

  // Respond to the lookup request with the same hash suffix.
  response_.hash_suffix = "";
  bool ok = base::HexStringToString("33445566", &response_.hash_suffix);
  CHECK(ok);
  SimulateResponse();

  // SCT should not be reported.
  EXPECT_EQ(url_loader_factory_.NumPending(), 0);
  histograms.ExpectUniqueSample(
      "Security.SCTAuditing.OptOut.LookupQueryResult",
      SCTAuditingReporter::LookupQueryResult::kSCTSuffixFound, 1);
}

// Tests that a hashdance lookup with a server error retries.
TEST_F(SCTAuditingReporterTest, HashdanceLookupServerError) {
  SCTAuditingReporter reporter = MakeReporter();
  reporter.Start();

  EXPECT_EQ(url_loader_factory_.NumPending(), 1);
  TestURLLoaderFactory::PendingRequest* pending_request =
      url_loader_factory_.GetPendingRequest(0);
  EXPECT_TRUE(pending_request->request.url.DomainIs(kTestLookupDomain));

  // Respond to the lookup request with an error.
  response_.status = "ERROR";
  SimulateResponse();
  histograms.ExpectUniqueSample(
      "Security.SCTAuditing.OptOut.LookupQueryResult",
      SCTAuditingReporter::LookupQueryResult::kStatusNotOk, 1);

  // A retry should be rescheduled.
  EXPECT_EQ(url_loader_factory_.NumPending(), 1);
  pending_request = url_loader_factory_.GetPendingRequest(0);
  EXPECT_TRUE(pending_request->request.url.DomainIs(kTestLookupDomain));

  // Respond to the lookup request with success.
  response_.status = "OK";
  SimulateResponse();

  // SCT should be reported.
  EXPECT_EQ(url_loader_factory_.NumPending(), 1);
  pending_request = url_loader_factory_.GetPendingRequest(0);
  EXPECT_EQ(pending_request->request.url.spec(), kTestReportURL);
}

// Tests that a hashdance lookup with an HTTP server error retries.
TEST_F(SCTAuditingReporterTest, HashdanceLookupHTTPError) {
  SCTAuditingReporter reporter = MakeReporter();
  reporter.Start();

  EXPECT_EQ(url_loader_factory_.NumPending(), 1);
  TestURLLoaderFactory::PendingRequest* pending_request =
      url_loader_factory_.GetPendingRequest(0);
  EXPECT_TRUE(pending_request->request.url.DomainIs(kTestLookupDomain));

  // Respond to the lookup request with an error.
  url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), /*content=*/"",
      net::HTTP_TOO_MANY_REQUESTS);
  histograms.ExpectUniqueSample(
      "Security.SCTAuditing.OptOut.LookupQueryResult",
      SCTAuditingReporter::LookupQueryResult::kHTTPError, 1);

  // A retry should be rescheduled.
  EXPECT_EQ(url_loader_factory_.NumPending(), 1);
  pending_request = url_loader_factory_.GetPendingRequest(0);
  EXPECT_TRUE(pending_request->request.url.DomainIs(kTestLookupDomain));

  // Respond to the lookup request with success.
  SimulateResponse();

  // SCT should be reported.
  EXPECT_EQ(url_loader_factory_.NumPending(), 1);
  pending_request = url_loader_factory_.GetPendingRequest(0);
  EXPECT_EQ(pending_request->request.url.spec(), kTestReportURL);
}

// Tests that a hashdance lookup with a server "now" timestamp past the expiry
// date does not get reported.
TEST_F(SCTAuditingReporterTest, HashdanceLookupCertificateExpired) {
  SCTAuditingReporter reporter = MakeReporter();
  reporter.Start();

  EXPECT_EQ(url_loader_factory_.NumPending(), 1);
  TestURLLoaderFactory::PendingRequest* pending_request =
      url_loader_factory_.GetPendingRequest(0);
  EXPECT_TRUE(pending_request->request.url.DomainIs(kTestLookupDomain));

  // Respond to the lookup request with a timestamp past the cert expiry.
  response_.now = reporter_metadata_.certificate_expiry + base::Seconds(1);
  SimulateResponse();

  // SCT should not be reported.
  EXPECT_EQ(url_loader_factory_.NumPending(), 0);
  histograms.ExpectUniqueSample(
      "Security.SCTAuditing.OptOut.LookupQueryResult",
      SCTAuditingReporter::LookupQueryResult::kCertificateExpired, 1);
}

// Tests that a hashdance lookup that does not return the SCT Log ID gets
// rescheduled.
TEST_F(SCTAuditingReporterTest, HashdanceLookupUnknownLog) {
  SCTAuditingReporter reporter = MakeReporter();
  reporter.Start();

  EXPECT_EQ(url_loader_factory_.NumPending(), 1);
  TestURLLoaderFactory::PendingRequest* pending_request =
      url_loader_factory_.GetPendingRequest(0);
  EXPECT_TRUE(pending_request->request.url.DomainIs(kTestLookupDomain));

  // Respond to the lookup request with a different log id.
  response_.log_id = "some_other_log";
  SimulateResponse();
  histograms.ExpectUniqueSample(
      "Security.SCTAuditing.OptOut.LookupQueryResult",
      SCTAuditingReporter::LookupQueryResult::kLogNotFound, 1);

  // A retry should be rescheduled.
  EXPECT_EQ(url_loader_factory_.NumPending(), 1);
  pending_request = url_loader_factory_.GetPendingRequest(0);
  EXPECT_TRUE(pending_request->request.url.DomainIs(kTestLookupDomain));

  // Respond to the lookup request with success.
  response_.log_id = "log_id";
  SimulateResponse();

  // SCT should be reported.
  EXPECT_EQ(url_loader_factory_.NumPending(), 1);
  pending_request = url_loader_factory_.GetPendingRequest(0);
  EXPECT_EQ(pending_request->request.url.spec(), kTestReportURL);
}

// Tests that a hashdance lookup indicating the log has not yet been ingested is
// rescheduled.
TEST_F(SCTAuditingReporterTest, HashdanceLookupLogNotIngested) {
  SCTAuditingReporter reporter = MakeReporter();
  reporter.Start();

  EXPECT_EQ(url_loader_factory_.NumPending(), 1);
  TestURLLoaderFactory::PendingRequest* pending_request =
      url_loader_factory_.GetPendingRequest(0);
  EXPECT_TRUE(pending_request->request.url.DomainIs(kTestLookupDomain));

  // Respond to the lookup request with a too early `ingested_until`.
  response_.ingested_until = reporter_metadata_.issued - base::Seconds(1);
  SimulateResponse();
  histograms.ExpectUniqueSample(
      "Security.SCTAuditing.OptOut.LookupQueryResult",
      SCTAuditingReporter::LookupQueryResult::kLogNotYetIngested, 1);

  // A retry should be rescheduled.
  EXPECT_EQ(url_loader_factory_.NumPending(), 1);
  pending_request = url_loader_factory_.GetPendingRequest(0);
  EXPECT_TRUE(pending_request->request.url.DomainIs(kTestLookupDomain));

  // Respond to the lookup request with success.
  response_.ingested_until = reporter_metadata_.issued + base::Seconds(1);
  SimulateResponse();

  // SCT should be reported.
  EXPECT_EQ(url_loader_factory_.NumPending(), 1);
  pending_request = url_loader_factory_.GetPendingRequest(0);
  EXPECT_EQ(pending_request->request.url.spec(), kTestReportURL);
}

// Tests that if chrome thinks the SCT may not have been ingested by Google, it
// will be scheduled for after some reasonable delay.
TEST_F(SCTAuditingReporterTest, HashdanceSCTSuspectedNotYetIngested) {
  reporter_metadata_.issued = base::Time::Now();
  SCTAuditingReporter reporter = MakeReporter();
  reporter.Start();

  // No request should be made. Instead, it should have been scheduled for
  // kExpectedIngestionDelay + (0..kMaxIngestionRandomDelay).
  // For this test, kMaxIngestionRandomDelay is zero.
  EXPECT_EQ(url_loader_factory_.NumPending(), 0);
  task_environment_.FastForwardBy(kExpectedIngestionDelay);

  EXPECT_EQ(url_loader_factory_.NumPending(), 1);
  TestURLLoaderFactory::PendingRequest* pending_request =
      url_loader_factory_.GetPendingRequest(0);
  EXPECT_TRUE(pending_request->request.url.DomainIs(kTestLookupDomain));

  // Respond to the lookup request.
  response_.now = base::Time::Now();
  response_.ingested_until = base::Time::Now();
  SimulateResponse();

  // SCT should be reported.
  EXPECT_EQ(url_loader_factory_.NumPending(), 1);
  pending_request = url_loader_factory_.GetPendingRequest(0);
  EXPECT_EQ(pending_request->request.url.spec(), kTestReportURL);
  histograms.ExpectUniqueSample(
      "Security.SCTAuditing.OptOut.LookupQueryResult",
      SCTAuditingReporter::LookupQueryResult::kSCTSuffixNotFound, 1);
}

}  // namespace network
