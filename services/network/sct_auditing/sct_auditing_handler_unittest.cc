// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/sct_auditing/sct_auditing_handler.h"

#include <memory>
#include <string_view>

#include "base/base64.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/hash_value.h"
#include "net/cert/ct_serialization.h"
#include "net/cert/merkle_tree_leaf.h"
#include "net/cert/sct_status_flags.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/proto/sct_audit_report.pb.h"
#include "services/network/sct_auditing/sct_auditing_cache.h"
#include "services/network/sct_auditing/sct_auditing_reporter.h"
#include "services/network/test/fake_test_cert_verifier_params_factory.h"
#include "services/network/test/test_network_context_client.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "services/network/url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/utility/utility.h"

namespace network {

namespace {

// The particular value of the log ID doesn't matter; it just has to be the
// correct length.
constexpr uint8_t kTestLogId[] = {
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01};
const std::string kTestLogIdAsString(reinterpret_cast<const char*>(kTestLogId),
                                     sizeof(kTestLogId));

constexpr base::TimeDelta kTestLogMMD = base::Seconds(42);

constexpr base::TimeDelta kTestHWMPeriod = base::Seconds(1);

class SCTAuditingHandlerTest : public testing::Test {
 public:
  SCTAuditingHandlerTest()
      : network_service_(NetworkService::CreateForTesting()) {}
  ~SCTAuditingHandlerTest() override = default;

  SCTAuditingHandlerTest(const SCTAuditingHandlerTest&) = delete;
  SCTAuditingHandlerTest& operator=(const SCTAuditingHandlerTest&) = delete;

  void SetUp() override {
    ASSERT_TRUE(persistence_dir_.CreateUniqueTempDir());
    persistence_path_ = persistence_dir_.GetPath().AppendASCII("SCT Auditing");
    SCTAuditingReporter::SetRetryDelayForTesting(base::TimeDelta());

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

    // Set up fake CT logs.
    mojom::CTLogInfoPtr log(std::in_place);
    log->id = kTestLogIdAsString;
    log->mmd = kTestLogMMD;
    std::vector<mojom::CTLogInfoPtr> log_list;
    log_list.emplace_back(std::move(log));
    base::RunLoop run_loop;
    network_service_->UpdateCtLogList(std::move(log_list),
                                      run_loop.QuitClosure());
    run_loop.Run();

    // A NetworkContextClient is needed for querying/updating the report count.
    mojo::PendingRemote<network::mojom::NetworkContextClient>
        network_context_client_remote;
    network_context_client_ =
        std::make_unique<network::TestNetworkContextClient>(
            network_context_client_remote.InitWithNewPipeAndPassReceiver());
    network_context_->SetClient(std::move(network_context_client_remote));

    // Set up SCT auditing configuration.
    auto* cache = network_service_->sct_auditing_cache();
    mojom::SCTAuditingConfigurationPtr configuration(std::in_place);
    configuration->sampling_rate = 1.0;
    configuration->report_uri = GURL("https://example.test");
    configuration->traffic_annotation =
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
    configuration->hashdance_traffic_annotation =
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
    cache->Configure(std::move(configuration));

    chain_ =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
    ASSERT_TRUE(chain_.get());

    handler_ = std::make_unique<SCTAuditingHandler>(network_context_.get(),
                                                    persistence_path_);
    handler_->set_hwm_metrics_period_for_testing(kTestHWMPeriod);
    handler_->SetMode(mojom::SCTAuditingMode::kEnhancedSafeBrowsingReporting);

    mojo::PendingRemote<network::mojom::URLLoaderFactory> factory_remote;
    url_loader_factory_.Clone(factory_remote.InitWithNewPipeAndPassReceiver());
    handler_->SetURLLoaderFactoryForTesting(std::move(factory_remote));

    // Clear out any pending tasks before starting tests.
    task_environment_.RunUntilIdle();
  }

  // Get the contents of `persistence_path_`. Pumps the message loop before
  // returning the result.
  std::string GetTestFileContents() {
    task_environment_.RunUntilIdle();
    std::string file_contents;
    base::ReadFileToString(persistence_path_, &file_contents);
    return file_contents;
  }

  // Check whether `substring` appears in the file contents at
  // `persistence_path_`.
  bool FileContentsHasString(const std::string& substring) {
    auto contents = GetTestFileContents();
    auto position = contents.find(substring);
    return position != std::string::npos;
  }

  // Waits for `expected_requests` to be seen by the TestURLLoaderFactory. Note
  // that this only counts HTTP requests, so network errors (e.g., cert errors)
  // won't count.
  void WaitForRequests(size_t expected_requests) {
    // Initialize a new RunLoop, so that tests can call WaitForRequests()
    // multiple times, if needed.
    run_loop_ = std::make_unique<base::RunLoop>();

    if (num_requests_seen_ >= expected_requests) {
      return;
    }

    // Add a TestURLLoaderFactory interceptor to count requests seen.
    url_loader_factory_.SetInterceptor(base::BindLambdaForTesting(
        [&](const network::ResourceRequest& request) {
          ++num_requests_seen_;
          if (run_loop_->running() && num_requests_seen_ >= expected_requests) {
            run_loop_->QuitWhenIdle();
          }
        }));

    run_loop_->Run();
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir persistence_dir_;
  base::FilePath persistence_path_;
  std::unique_ptr<NetworkService> network_service_;
  std::unique_ptr<NetworkContext> network_context_;
  std::unique_ptr<TestNetworkContextClient> network_context_client_;
  scoped_refptr<net::X509Certificate> chain_;
  std::unique_ptr<SCTAuditingHandler> handler_;
  TestURLLoaderFactory url_loader_factory_;

  std::unique_ptr<base::RunLoop> run_loop_;
  size_t num_requests_seen_ = 0;

  // Stores the mojo::Remote<mojom::NetworkContext> of the most recently created
  // NetworkContext.
  mojo::Remote<mojom::NetworkContext> network_context_remote_;
};

// Constructs a net::SignedCertificateTimestampAndStatus with the given
// information and appends it to |sct_list|.
void MakeTestSCTAndStatus(
    net::ct::SignedCertificateTimestamp::Origin origin,
    const std::string& extensions,
    const std::string& signature_data,
    const base::Time& timestamp,
    net::ct::SCTVerifyStatus status,
    net::SignedCertificateTimestampAndStatusList* sct_list) {
  scoped_refptr<net::ct::SignedCertificateTimestamp> sct(
      new net::ct::SignedCertificateTimestamp());
  sct->version = net::ct::SignedCertificateTimestamp::V1;
  sct->log_id = kTestLogIdAsString;

  sct->extensions = extensions;
  sct->timestamp = timestamp;
  sct->signature.signature_data = signature_data;
  sct->origin = origin;
  sct_list->push_back(net::SignedCertificateTimestampAndStatus(sct, status));
}

// Tests that if reporting is disabled, reports are not created.
TEST_F(SCTAuditingHandlerTest, DisableReporting) {
  // Create a report which would normally trigger a send.
  const net::HostPortPair host_port_pair("example.com", 443);
  net::SignedCertificateTimestampAndStatusList sct_list;
  MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                       "extensions", "signature", base::Time::Now(),
                       net::ct::SCT_STATUS_OK, &sct_list);
  handler_->SetMode(mojom::SCTAuditingMode::kDisabled);
  handler_->MaybeEnqueueReport(host_port_pair, chain_.get(), sct_list);

  // Check that there are no pendin reports.
  EXPECT_EQ(0u, handler_->GetPendingReportersForTesting()->size());
}

// Tests that when a new report is sampled, it will be sent to the server.
// TODO(cthomp): Allow tracking success/failure of the report being sent. One
// way would be to have OnSuccess/OnError handlers installed on the handler.
TEST_F(SCTAuditingHandlerTest, ReportsSentWithServerOK) {
  // Enqueue a report which will trigger a send.
  const net::HostPortPair host_port_pair("example.com", 443);
  net::SignedCertificateTimestampAndStatusList sct_list;
  MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                       "extensions", "signature", base::Time::Now(),
                       net::ct::SCT_STATUS_OK, &sct_list);
  handler_->MaybeEnqueueReport(host_port_pair, chain_.get(), sct_list);

  // Check that there is one pending report.
  EXPECT_EQ(1u, handler_->GetPendingReportersForTesting()->size());

  // Wait for initial request.
  WaitForRequests(1u);

  // Simulate the server returning 200 OK to the report request.
  // The request must already be pending before calling this.
  url_loader_factory_.SimulateResponseForPendingRequest(
      "https://example.test",
      /*content=*/"",
      /*status=*/net::HTTP_OK);

  EXPECT_EQ(0, url_loader_factory_.NumPending());

  // Check that the pending reporter was deleted on successful completion.
  EXPECT_TRUE(handler_->GetPendingReportersForTesting()->empty());
}

// Tests when the report server returns an HTTP error code.
TEST_F(SCTAuditingHandlerTest, ReportSentWithServerError) {
  // Set a long retry delay to allow inspecting the handler between an error and
  // resending the report.
  SCTAuditingReporter::SetRetryDelayForTesting(base::Seconds(1));

  // Enqueue a report which will trigger a send.
  const net::HostPortPair host_port_pair("example.com", 443);
  net::SignedCertificateTimestampAndStatusList sct_list;
  MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                       "extensions", "signature", base::Time::Now(),
                       net::ct::SCT_STATUS_OK, &sct_list);
  handler_->MaybeEnqueueReport(host_port_pair, chain_.get(), sct_list);

  // Check that there is one pending report.
  EXPECT_EQ(1u, handler_->GetPendingReportersForTesting()->size());

  // Wait for initial request.
  WaitForRequests(1u);

  // Simulate the server returning 429 TOO MANY REQUEST to the report request.
  // The request must already be pending before calling this.
  url_loader_factory_.SimulateResponseForPendingRequest(
      "https://example.test",
      /*content=*/"",
      /*status=*/net::HTTP_TOO_MANY_REQUESTS);

  EXPECT_EQ(0, url_loader_factory_.NumPending());

  // The pending reporter will remain, awaiting retry.
  EXPECT_EQ(1u, handler_->GetPendingReportersForTesting()->size());
}

// Connections that have a mix of valid and invalid SCTs should only include the
// valid SCTs in the report.
TEST_F(SCTAuditingHandlerTest, ReportsOnlyIncludesValidSCTs) {
  // Add a report with different types and validities of SCTs.
  const net::HostPortPair host_port_pair("example.com", 443);
  net::SignedCertificateTimestampAndStatusList sct_list;
  MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                       "extensions1", "valid_signature", base::Time::Now(),
                       net::ct::SCT_STATUS_OK, &sct_list);
  MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                       "extensions2", "invalid_signature", base::Time::Now(),
                       net::ct::SCT_STATUS_INVALID_SIGNATURE, &sct_list);
  MakeTestSCTAndStatus(
      net::ct::SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION,
      "extensions3", "invalid_log", base::Time::Now(),
      net::ct::SCT_STATUS_LOG_UNKNOWN, &sct_list);
  handler_->MaybeEnqueueReport(host_port_pair, chain_.get(), sct_list);

  auto* pending_reporters = handler_->GetPendingReportersForTesting();
  ASSERT_EQ(1u, pending_reporters->size());

  // No invalid SCTs should be in any of the pending reports.
  for (const auto& reporter : *pending_reporters) {
    for (auto& sct_and_status :
         reporter.second->report()->certificate_report(0).included_sct()) {
      // Decode the SCT and check that only the valid SCT was included.
      std::string_view encoded_sct(sct_and_status.serialized_sct());
      scoped_refptr<net::ct::SignedCertificateTimestamp> decoded_sct;
      ASSERT_TRUE(net::ct::DecodeSignedCertificateTimestamp(&encoded_sct,
                                                            &decoded_sct));
      EXPECT_EQ("valid_signature", decoded_sct->signature.signature_data);
    }
  }
}

// If operating on hashdance mode, calculate and store the SCT leaf hash and
// append log metadata.
TEST_F(SCTAuditingHandlerTest, PopulateSCTMetadataOnHashdanceMode) {
  base::HistogramTester histograms;
  const net::HostPortPair host_port_pair("example.com", 443);
  net::SignedCertificateTimestampAndStatusList sct_list;
  const base::Time issued = base::Time::Now();
  MakeTestSCTAndStatus(
      net::ct::SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION, "extensions",
      "valid_signature", issued, net::ct::SCT_STATUS_OK, &sct_list);

  for (mojom::SCTAuditingMode mode :
       {mojom::SCTAuditingMode::kEnhancedSafeBrowsingReporting,
        mojom::SCTAuditingMode::kHashdance}) {
    SCOPED_TRACE(testing::Message() << "Mode: " << static_cast<int>(mode));
    handler_->SetMode(mode);
    handler_->MaybeEnqueueReport(host_port_pair, chain_.get(), sct_list);
    auto* pending_reporters = handler_->GetPendingReportersForTesting();
    ASSERT_EQ(pending_reporters->size(), 1u);
    for (const auto& reporter : *pending_reporters) {
      if (mode == mojom::SCTAuditingMode::kHashdance) {
        net::ct::MerkleTreeLeaf merkle_tree_leaf;
        std::string expected_leaf_hash;
        ASSERT_TRUE(net::ct::GetMerkleTreeLeaf(
            chain_.get(), sct_list.at(0).sct.get(), &merkle_tree_leaf));
        ASSERT_TRUE(
            net::ct::HashMerkleTreeLeaf(merkle_tree_leaf, &expected_leaf_hash));

        const SCTAuditingReporter::SCTHashdanceMetadata& metadata =
            *reporter.second->sct_hashdance_metadata();
        EXPECT_EQ(metadata.leaf_hash, expected_leaf_hash);
        EXPECT_EQ(metadata.log_id, kTestLogIdAsString);
        EXPECT_EQ(metadata.log_mmd, kTestLogMMD);
        EXPECT_EQ(metadata.issued, issued);
        EXPECT_EQ(metadata.certificate_expiry, chain_->valid_expiry());
      } else {
        EXPECT_FALSE(reporter.second->sct_hashdance_metadata());
      }
    }
    histograms.ExpectUniqueSample(
        "Security.SCTAuditing.OptOut.PopularSCTSkipped", false,
        mode == mojom::SCTAuditingMode::kHashdance ? 1 : 0);

    // Reset by clearing all pending reports and cache entries.
    base::RunLoop run_loop;
    handler_->ClearPendingReports(run_loop.QuitClosure());
    network_service_->sct_auditing_cache()->ClearCache();
    run_loop.Run();
  }
}

// If operating on hashdance mode, do not report popular SCTs.
TEST_F(SCTAuditingHandlerTest, DoNotReportPopularSCT) {
  const net::HostPortPair host_port_pair("example.com", 443);
  net::SignedCertificateTimestampAndStatusList sct_list;
  MakeTestSCTAndStatus(
      net::ct::SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION, "extensions",
      "valid_signature", base::Time::Now(), net::ct::SCT_STATUS_OK, &sct_list);
  net::ct::MerkleTreeLeaf merkle_tree_leaf;
  std::string leaf_hash_string;
  ASSERT_TRUE(net::ct::GetMerkleTreeLeaf(chain_.get(), sct_list.at(0).sct.get(),
                                         &merkle_tree_leaf));
  ASSERT_TRUE(net::ct::HashMerkleTreeLeaf(merkle_tree_leaf, &leaf_hash_string));
  std::vector<uint8_t> leaf_hash(leaf_hash_string.begin(),
                                 leaf_hash_string.end());

  // Create a list of sorted leaf hashes that will contain the SCT's leaf hash.
  std::vector<std::vector<uint8_t>> leaf_hashes;
  for (size_t byte = 0; byte < 256; ++byte) {
    std::vector<uint8_t> new_leaf_hash = leaf_hash;
    new_leaf_hash[0] = byte;
    leaf_hashes.emplace_back(std::move(new_leaf_hash));
  }

  network_service_->sct_auditing_cache()->set_popular_scts({leaf_hash});

  for (mojom::SCTAuditingMode mode :
       {mojom::SCTAuditingMode::kEnhancedSafeBrowsingReporting,
        mojom::SCTAuditingMode::kHashdance}) {
    SCOPED_TRACE(testing::Message() << "Mode: " << static_cast<int>(mode));
    base::HistogramTester histograms;
    handler_->SetMode(mode);
    handler_->MaybeEnqueueReport(host_port_pair, chain_.get(), sct_list);
    auto* pending_reporters = handler_->GetPendingReportersForTesting();
    EXPECT_EQ(pending_reporters->size(),
              mode == mojom::SCTAuditingMode::kHashdance ? 0u : 1u);

    // The hashdance request should record a count for PopularSCTSkipped.
    histograms.ExpectUniqueSample(
        "Security.SCTAuditing.OptOut.PopularSCTSkipped", true,
        mode == mojom::SCTAuditingMode::kHashdance ? 1 : 0);

    // Reset by clearing all pending reports and cache entries.
    base::RunLoop run_loop;
    handler_->ClearPendingReports(run_loop.QuitClosure());
    network_service_->sct_auditing_cache()->ClearCache();
    run_loop.Run();
  }
}

// Tests a single retry. The server initially returns an error, but then returns
// OK the second try.
TEST_F(SCTAuditingHandlerTest, ReportSucceedsOnSecondTry) {
  base::HistogramTester histograms;

  // Enqueue a report which will trigger a send.
  const net::HostPortPair host_port_pair("example.com", 443);
  net::SignedCertificateTimestampAndStatusList sct_list;
  MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                       "extensions", "signature", base::Time::Now(),
                       net::ct::SCT_STATUS_OK, &sct_list);
  handler_->MaybeEnqueueReport(host_port_pair, chain_.get(), sct_list);

  // Check that there is one pending report.
  EXPECT_EQ(1u, handler_->GetPendingReportersForTesting()->size());

  // Wait for initial request.
  WaitForRequests(1u);

  // Simulate the server returning 429 TOO MANY REQUEST to the report request.
  // The request must already be pending before calling this.
  url_loader_factory_.SimulateResponseForPendingRequest(
      "https://example.test",
      /*content=*/"",
      /*status=*/net::HTTP_TOO_MANY_REQUESTS);

  // The retry timer is set to zero, so a retry should have been scheduled
  // already.
  EXPECT_EQ(1, url_loader_factory_.NumPending());

  // With retry enabled, the pending reporter should remain on failure.
  EXPECT_EQ(1u, handler_->GetPendingReportersForTesting()->size());

  // Simulate the server returning 200 OK to the report request.
  url_loader_factory_.SimulateResponseForPendingRequest(
      "https://example.test",
      /*content=*/"",
      /*status=*/net::HTTP_OK);

  // Wait for second request.
  WaitForRequests(2u);

  EXPECT_EQ(0, url_loader_factory_.NumPending());

  // Check that the pending reporter was deleted on successful completion.
  EXPECT_TRUE(handler_->GetPendingReportersForTesting()->empty());

  histograms.ExpectUniqueSample(
      "Security.SCTAuditing.OptIn.ReportCompletionStatus",
      SCTAuditingReporter::CompletionStatus::kSuccessAfterRetries, 1);
}

// Tests that after max_tries, the reporter stops and is deleted.
TEST_F(SCTAuditingHandlerTest, ExhaustAllRetriesShouldDeleteReporter) {
  base::HistogramTester histograms;

  // Enqueue a report which will trigger a send.
  const net::HostPortPair host_port_pair("example.com", 443);
  net::SignedCertificateTimestampAndStatusList sct_list;
  MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                       "extensions", "signature", base::Time::Now(),
                       net::ct::SCT_STATUS_OK, &sct_list);
  handler_->MaybeEnqueueReport(host_port_pair, chain_.get(), sct_list);

  // Check that there is one pending reporter.
  EXPECT_EQ(1u, handler_->GetPendingReportersForTesting()->size());

  // Simulate the server returning 429 TOO MANY REQUEST to every request
  url_loader_factory_.AddResponse("https://example.test",
                                  /*content=*/"",
                                  /*status=*/net::HTTP_TOO_MANY_REQUESTS);

  // Wait for initial request + 15 retries.
  WaitForRequests(16u);

  // The reporter should be deleted when it runs out of retries.
  EXPECT_TRUE(handler_->GetPendingReportersForTesting()->empty());

  // The Reporter should send 16 requests: 1 initial attempt, and 15 retries
  // (the default max_retries for SCTAuditingReporter).
  EXPECT_EQ(16u, num_requests_seen_);

  histograms.ExpectUniqueSample(
      "Security.SCTAuditing.OptIn.ReportCompletionStatus",
      SCTAuditingReporter::CompletionStatus::kRetriesExhausted, 1);
}

// Tests that report completion metrics are correctly recorded when a report
// succeeds on the first try.
TEST_F(SCTAuditingHandlerTest, RetriesEnabledSucceedFirstTryMetrics) {
  base::HistogramTester histograms;

  // Enqueue a report which will trigger a send.
  const net::HostPortPair host_port_pair("example.com", 443);
  net::SignedCertificateTimestampAndStatusList sct_list;
  MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                       "extensions", "signature", base::Time::Now(),
                       net::ct::SCT_STATUS_OK, &sct_list);
  handler_->MaybeEnqueueReport(host_port_pair, chain_.get(), sct_list);

  // Wait for the initial request to be pending.
  WaitForRequests(1u);

  EXPECT_EQ(1, url_loader_factory_.NumPending());

  // Simulate the server returning 200 OK to the report request.
  url_loader_factory_.SimulateResponseForPendingRequest(
      "https://example.test",
      /*content=*/"",
      /*status=*/net::HTTP_OK);

  // "Success on first try" should be logged to the histogram.
  histograms.ExpectUniqueSample(
      "Security.SCTAuditing.OptIn.ReportCompletionStatus",
      SCTAuditingReporter::CompletionStatus::kSuccessFirstTry, 1);
}

TEST_F(SCTAuditingHandlerTest, ReportHighWaterMarkMetrics) {
  base::HistogramTester histograms;

  // Send two reports.
  const net::HostPortPair host_port_pair("example.com", 443);
  net::SignedCertificateTimestampAndStatusList sct_list1;
  MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                       "extensions1", "signature1", base::Time::Now(),
                       net::ct::SCT_STATUS_OK, &sct_list1);
  handler_->MaybeEnqueueReport(host_port_pair, chain_.get(), sct_list1);

  net::SignedCertificateTimestampAndStatusList sct_list2;
  MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                       "extensions2", "signature2", base::Time::Now(),
                       net::ct::SCT_STATUS_OK, &sct_list2);
  handler_->MaybeEnqueueReport(host_port_pair, chain_.get(), sct_list2);

  EXPECT_EQ(2u, handler_->GetPendingReportersForTesting()->size());

  task_environment_.FastForwardBy(kTestHWMPeriod);

  // The bucket for a HWM of 2 should have a single sample.
  histograms.ExpectUniqueSample("Security.SCTAuditing.OptIn.ReportersHWM", 2,
                                1);
}

// If a report doesn't have any valid SCTs, it should not get sent at all.
TEST_F(SCTAuditingHandlerTest, ReportNotGeneratedIfNoValidSCTs) {
  const net::HostPortPair host_port_pair("example.com", 443);
  net::SignedCertificateTimestampAndStatusList sct_list;
  MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                       "extensions", "signature", base::Time::Now(),
                       net::ct::SCT_STATUS_INVALID_SIGNATURE, &sct_list);
  handler_->MaybeEnqueueReport(host_port_pair, chain_.get(), sct_list);

  EXPECT_EQ(0u, handler_->GetPendingReportersForTesting()->size());
}

// Test that when the SCTAuditingHandler is created without a persistence path
// (e.g., as happens for ephemeral profiles), no file writer is created.
TEST_F(SCTAuditingHandlerTest, HandlerWithoutPersistencePath) {
  mojo::PendingRemote<network::mojom::URLLoaderFactory> factory_remote;
  url_loader_factory_.Clone(factory_remote.InitWithNewPipeAndPassReceiver());

  // Set up a Handler with an empty `persistence_path`.
  SCTAuditingHandler handler(network_context_.get(), base::FilePath());
  handler.SetMode(mojom::SCTAuditingMode::kEnhancedSafeBrowsingReporting);
  handler.SetURLLoaderFactoryForTesting(std::move(factory_remote));

  // `file_writer` should not be created for this handler.
  auto* file_writer = handler.GetFileWriterForTesting();
  ASSERT_EQ(file_writer, nullptr);
}

// Test that when the SCTAuditingHandler is created with a valid persistence
// path, then pending reports get stored to disk.
TEST_F(SCTAuditingHandlerTest, HandlerWithPersistencePath) {
  mojo::PendingRemote<network::mojom::URLLoaderFactory> factory_remote;
  url_loader_factory_.Clone(factory_remote.InitWithNewPipeAndPassReceiver());

  SCTAuditingHandler handler(network_context_.get(), persistence_path_);
  handler.SetMode(mojom::SCTAuditingMode::kEnhancedSafeBrowsingReporting);
  handler.SetURLLoaderFactoryForTesting(std::move(factory_remote));

  auto* file_writer = handler.GetFileWriterForTesting();
  ASSERT_TRUE(file_writer);

  // Add a Reporter to the Handler and check that it gets scheduled to be
  // persisted to disk.
  auto report = std::make_unique<sct_auditing::SCTClientReport>();
  auto* tls_report = report->add_certificate_report();
  auto* connection_context = tls_report->mutable_context();
  auto* origin = connection_context->mutable_origin();
  origin->set_hostname("example.test");
  origin->set_port(443);

  // Fake a HashValue to use as the key.
  net::HashValue reporter_key(net::HASH_VALUE_SHA256);

  handler.AddReporter(reporter_key, std::move(report), std::nullopt);
  ASSERT_EQ(handler.GetPendingReportersForTesting()->size(), 1u);
  ASSERT_TRUE(file_writer->HasPendingWrite());

  // Check that file got written with the expected content.
  file_writer->DoScheduledWrite();
  ASSERT_FALSE(file_writer->HasPendingWrite());
  EXPECT_TRUE(FileContentsHasString(reporter_key.ToString()));

  WaitForRequests(1u);

  EXPECT_EQ(1, url_loader_factory_.NumPending());

  // Simulate the server returning 200 OK to the report request.
  url_loader_factory_.SimulateResponseForPendingRequest(
      "https://example.test",
      /*content=*/"",
      /*status=*/net::HTTP_OK);

  // Check that there are no pending requests anymore.
  EXPECT_EQ(0, url_loader_factory_.NumPending());

  // Check that the pending reporter was deleted on successful completion.
  EXPECT_TRUE(handler.GetPendingReportersForTesting()->empty());

  // Check that the Reporter is no longer in the file.
  file_writer->DoScheduledWrite();
  ASSERT_FALSE(file_writer->HasPendingWrite());
  EXPECT_FALSE(FileContentsHasString(reporter_key.ToString()));
}

// Tests that serializing reports and then deserializing them results in the
// same data.
TEST_F(SCTAuditingHandlerTest, DataRoundTrip) {
  // Create a Handler, add a reporter, and wait for it to get persisted.
  {
    SCTAuditingHandler handler(network_context_.get(), persistence_path_);
    handler.SetMode(mojom::SCTAuditingMode::kEnhancedSafeBrowsingReporting);
    mojo::PendingRemote<network::mojom::URLLoaderFactory> factory_remote;
    url_loader_factory_.Clone(factory_remote.InitWithNewPipeAndPassReceiver());
    handler.SetURLLoaderFactoryForTesting(std::move(factory_remote));

    auto* file_writer = handler.GetFileWriterForTesting();
    ASSERT_TRUE(file_writer);

    ASSERT_FALSE(file_writer->HasPendingWrite());

    // Add a Reporter to the Handler and check that it gets scheduled to be
    // persisted to disk.
    auto report = std::make_unique<sct_auditing::SCTClientReport>();
    auto* tls_report = report->add_certificate_report();
    auto* connection_context = tls_report->mutable_context();
    auto* origin = connection_context->mutable_origin();
    origin->set_hostname("example.test");
    origin->set_port(443);

    // Fake a HashValue to use as the key.
    net::HashValue reporter_key(net::HASH_VALUE_SHA256);

    SCTAuditingReporter::SCTHashdanceMetadata metadata;
    metadata.leaf_hash = "leaf hash";
    metadata.log_id = "log id";
    metadata.log_mmd = base::Seconds(42);
    metadata.issued = base::Time::UnixEpoch();
    metadata.certificate_expiry = base::Time::UnixEpoch() + base::Seconds(42);
    handler.AddReporter(reporter_key, std::move(report), std::move(metadata));
    ASSERT_EQ(handler.GetPendingReportersForTesting()->size(), 1u);
    ASSERT_TRUE(file_writer->HasPendingWrite());

    // Check that file got written with the expected content.
    file_writer->DoScheduledWrite();
    ASSERT_FALSE(file_writer->HasPendingWrite());
    EXPECT_TRUE(FileContentsHasString(reporter_key.ToString()));
  }

  // Create a second Handler using the same persistence path. It should load
  // the same data.
  {
    SCTAuditingHandler handler(network_context_.get(), persistence_path_);
    handler.SetMode(mojom::SCTAuditingMode::kEnhancedSafeBrowsingReporting);
    mojo::PendingRemote<network::mojom::URLLoaderFactory> factory_remote;
    url_loader_factory_.Clone(factory_remote.InitWithNewPipeAndPassReceiver());
    handler.SetURLLoaderFactoryForTesting(std::move(factory_remote));

    auto* file_writer = handler.GetFileWriterForTesting();
    ASSERT_TRUE(file_writer);

    WaitForRequests(1u);

    auto* pending_reporters = handler.GetPendingReportersForTesting();
    ASSERT_EQ(1u, pending_reporters->size());

    // Reporter should be for "example.test:443" as added in the first Handler.
    for (const auto& reporter : *pending_reporters) {
      auto origin =
          reporter.second->report()->certificate_report(0).context().origin();
      EXPECT_EQ(origin.hostname(), "example.test");
      EXPECT_EQ(origin.port(), 443);

      const std::optional<SCTAuditingReporter::SCTHashdanceMetadata>& metadata =
          reporter.second->sct_hashdance_metadata();
      ASSERT_TRUE(metadata);
      EXPECT_EQ(metadata->leaf_hash, "leaf hash");
      EXPECT_EQ(metadata->log_id, "log id");
      EXPECT_EQ(metadata->log_mmd, base::Seconds(42));
      EXPECT_EQ(metadata->issued, base::Time::UnixEpoch());
      EXPECT_EQ(metadata->certificate_expiry,
                base::Time::UnixEpoch() + base::Seconds(42));
    }
  }
}

// Test that deserializing bad data shouldn't result in any reporters being
// created.
TEST_F(SCTAuditingHandlerTest, DeserializeBadData) {
  base::HistogramTester histograms;

  mojo::PendingRemote<network::mojom::URLLoaderFactory> factory_remote;
  url_loader_factory_.Clone(factory_remote.InitWithNewPipeAndPassReceiver());

  // Set an empty persistence path so no file IO is performed.
  SCTAuditingHandler handler(network_context_.get(), base::FilePath());
  handler.SetURLLoaderFactoryForTesting(std::move(factory_remote));

  // Non-JSON data.
  handler.DeserializeData("Blorp");
  EXPECT_EQ(handler.GetPendingReportersForTesting()->size(), 0u);

  // JSON data but non-sensical.
  handler.DeserializeData("[15]");
  EXPECT_EQ(handler.GetPendingReportersForTesting()->size(), 0u);

  // JSON data in the right format, but with invalid keys.
  handler.DeserializeData(R"([{"blorp": "a", "bloop": "b", "bleep": "c"}])");
  EXPECT_EQ(handler.GetPendingReportersForTesting()->size(), 0u);

  // JSON data with the right format and keys, but data is invalid.
  handler.DeserializeData(
      R"([{"reporter_key": "a", "report": "b", "backoff_entry": ["c"]}])");
  EXPECT_EQ(handler.GetPendingReportersForTesting()->size(), 0u);

  // JSON data with invalid SCT metadata.
  handler.DeserializeData(
      R"(reporter_key":
        "sha256/qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqo=",
      "report": "EhUKExIRCgxleGFtcGxlLnRlc3QQuwM=",
      "sct_metadata": ":(",
      "backoff_entry": [2,1,"30000000","11644578625551798"])");
  EXPECT_EQ(handler.GetPendingReportersForTesting()->size(), 0u);

  // Check that no file got written to the persistence path.
  EXPECT_EQ(GetTestFileContents(), std::string());

  // Check that these deserializations resulted in logging the 0-bucket of the
  // NumPersistedReportsLoaded histogram. The count should be equal to the
  // number of calls to DeserializeData() above.
  histograms.ExpectUniqueSample(
      "Security.SCTAuditing.NumPersistedReportsLoaded", 0, 5);
}

// Test that a handler loads valid persisted data from disk and creates pending
// reporters for each entry.
TEST_F(SCTAuditingHandlerTest, HandlerWithExistingPersistedData) {
  base::HistogramTester histograms;

  // Set up previously persisted data on disk:
  // - Default-initialized net::HashValue(net::HASH_VALUE_SHA256)
  // - Empty SCTClientReport for origin "example.test:443".
  // - A simple BackoffEntry.
  std::string persisted_report =
      R"(
        [{
          "reporter_key":
            "sha256/qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqo=",
          "report": "EhUKExIRCgxleGFtcGxlLnRlc3QQuwM=",
          "backoff_entry": [2,0,"30000000","11644578625551798"]
        }]
      )";
  ASSERT_TRUE(base::WriteFile(persistence_path_, persisted_report));

  mojo::PendingRemote<network::mojom::URLLoaderFactory> factory_remote;
  url_loader_factory_.Clone(factory_remote.InitWithNewPipeAndPassReceiver());

  SCTAuditingHandler handler(network_context_.get(), persistence_path_);
  handler.SetMode(mojom::SCTAuditingMode::kEnhancedSafeBrowsingReporting);
  handler.SetURLLoaderFactoryForTesting(std::move(factory_remote));

  auto* file_writer = handler.GetFileWriterForTesting();
  ASSERT_TRUE(file_writer);

  WaitForRequests(1u);

  EXPECT_EQ(handler.GetPendingReportersForTesting()->size(), 1u);
  EXPECT_EQ(1, url_loader_factory_.NumPending());

  // Simulate the server returning 200 OK to the report request.
  url_loader_factory_.SimulateResponseForPendingRequest(
      "https://example.test",
      /*content=*/"",
      /*status=*/net::HTTP_OK);

  // Check that there is no pending requests anymore.
  EXPECT_EQ(0, url_loader_factory_.NumPending());

  // Check that the pending reporter was deleted on successful completion.
  EXPECT_TRUE(handler.GetPendingReportersForTesting()->empty());

  // Check that the Reporter is no longer in the file.
  file_writer->DoScheduledWrite();
  ASSERT_FALSE(file_writer->HasPendingWrite());
  EXPECT_FALSE(FileContentsHasString(
      "sha256/qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqo="));

  // Check that the NumPersistedReportsLoaded histogram was correctly logged.
  histograms.ExpectUniqueSample(
      "Security.SCTAuditing.NumPersistedReportsLoaded", 1, 1);
}

// Test that scheduling a retry causes the failure count to increment in
// persisted storage.
TEST_F(SCTAuditingHandlerTest, RetryUpdatesPersistedBackoffEntry) {
  // Set up previously persisted data on disk:
  // - Default-initialized net::HashValue(net::HASH_VALUE_SHA256)
  // - Empty SCTClientReport for origin "example.test:443".
  // - A simple BackoffEntry with a failure count of "1".
  std::string persisted_report =
      R"(
        [{
          "reporter_key":
            "sha256/qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqo=",
          "report": "EhUKExIRCgxleGFtcGxlLnRlc3QQuwM=",
          "backoff_entry": [2,1,"30000000","11644578625551798"]
        }]
      )";
  ASSERT_TRUE(base::WriteFile(persistence_path_, persisted_report));

  mojo::PendingRemote<network::mojom::URLLoaderFactory> factory_remote;
  url_loader_factory_.Clone(factory_remote.InitWithNewPipeAndPassReceiver());

  SCTAuditingHandler handler(network_context_.get(), persistence_path_);
  handler.SetMode(mojom::SCTAuditingMode::kEnhancedSafeBrowsingReporting);
  handler.SetURLLoaderFactoryForTesting(std::move(factory_remote));

  auto* file_writer = handler.GetFileWriterForTesting();
  ASSERT_TRUE(file_writer);

  WaitForRequests(1u);

  EXPECT_EQ(handler.GetPendingReportersForTesting()->size(), 1u);
  EXPECT_EQ(url_loader_factory_.NumPending(), 1);

  // Simulate the server returning error to the report request. The Reporter
  // should schedule a retry and trigger updating the persisted storage.
  url_loader_factory_.SimulateResponseForPendingRequest(
      "https://example.test",
      /*content=*/"",
      /*status=*/net::HTTP_TOO_MANY_REQUESTS);
  EXPECT_EQ(url_loader_factory_.NumPending(), 0);
  EXPECT_EQ(handler.GetPendingReportersForTesting()->size(), 1u);
  ASSERT_TRUE(file_writer->HasPendingWrite());

  // Check that the Reporter is updated in the persisted storage file.
  file_writer->DoScheduledWrite();
  ASSERT_FALSE(file_writer->HasPendingWrite());
  EXPECT_TRUE(FileContentsHasString(
      "sha256/qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqo="));
  // Persisted backoff entry should have the failure count incrememted to 2.
  // (The first value of a serialized BackoffEntry is the version, the second is
  // the failure count.)
  EXPECT_TRUE(FileContentsHasString(R"("backoff_entry":[2,2,)"));
}

// Test that retries carry over correctly. Specifically, a persisted entry with
// 14 retries already (one less than kMaxRetries), if after being loaded from
// persisted storage tries and fails once more, should get deleted.
TEST_F(SCTAuditingHandlerTest, RestoringMaxRetries) {
  // Set up previously persisted data on disk:
  // - Default-initialized net::HashValue(net::HASH_VALUE_SHA256)
  // - Empty SCTClientReport for origin "example.test:443".
  // - A simple BackoffEntry with a failure count of "15" (so it is scheduled to
  //   retry for the 15th and final time).
  std::string persisted_report =
      R"(
        [{
          "reporter_key":
            "sha256/qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqo=",
          "report": "EhUKExIRCgxleGFtcGxlLnRlc3QQuwM=",
          "backoff_entry": [2,15,"30000000","11644578625551798"]
        }]
      )";
  ASSERT_TRUE(base::WriteFile(persistence_path_, persisted_report));

  mojo::PendingRemote<network::mojom::URLLoaderFactory> factory_remote;
  url_loader_factory_.Clone(factory_remote.InitWithNewPipeAndPassReceiver());

  SCTAuditingHandler handler(network_context_.get(), persistence_path_);
  handler.SetMode(mojom::SCTAuditingMode::kEnhancedSafeBrowsingReporting);
  handler.SetURLLoaderFactoryForTesting(std::move(factory_remote));

  auto* file_writer = handler.GetFileWriterForTesting();
  ASSERT_TRUE(file_writer);

  WaitForRequests(1u);

  EXPECT_EQ(handler.GetPendingReportersForTesting()->size(), 1u);
  EXPECT_EQ(url_loader_factory_.NumPending(), 1);

  // Simulate the server returning error to the report request. The Reporter
  // should schedule a retry and trigger updating the persisted storage.
  url_loader_factory_.SimulateResponseForPendingRequest(
      "https://example.test",
      /*content=*/"",
      /*status=*/net::HTTP_TOO_MANY_REQUESTS);
  EXPECT_EQ(url_loader_factory_.NumPending(), 0);

  // Pending reporter should get deleted as it has reached max retries.
  EXPECT_EQ(handler.GetPendingReportersForTesting()->size(), 0u);

  // Reporter state on disk should get deleted as well.
  file_writer->DoScheduledWrite();
  ASSERT_FALSE(file_writer->HasPendingWrite());
  EXPECT_FALSE(FileContentsHasString(
      "sha256/qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqo="));
}

// Regression test for crbug.com/134432. For hashdance clients, when log list is
// empty the handler should just gracefully drop the report instead of crashing,
// and log a histogram for this case.
TEST_F(SCTAuditingHandlerTest, LogNotFound) {
  // Set up an empty CT log list.
  {
    std::vector<mojom::CTLogInfoPtr> log_list;
    base::RunLoop run_loop;
    network_service_->UpdateCtLogList(std::move(log_list),
                                      run_loop.QuitClosure());
    run_loop.Run();
  }

  const net::HostPortPair host_port_pair("example.com", 443);
  net::SignedCertificateTimestampAndStatusList sct_list;
  MakeTestSCTAndStatus(
      net::ct::SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION, "extensions",
      "valid_signature", base::Time::Now(), net::ct::SCT_STATUS_OK, &sct_list);
  net::ct::MerkleTreeLeaf merkle_tree_leaf;
  std::string leaf_hash_string;
  ASSERT_TRUE(net::ct::GetMerkleTreeLeaf(chain_.get(), sct_list.at(0).sct.get(),
                                         &merkle_tree_leaf));
  ASSERT_TRUE(net::ct::HashMerkleTreeLeaf(merkle_tree_leaf, &leaf_hash_string));
  std::vector<uint8_t> leaf_hash(leaf_hash_string.begin(),
                                 leaf_hash_string.end());

  for (mojom::SCTAuditingMode mode :
       {mojom::SCTAuditingMode::kEnhancedSafeBrowsingReporting,
        mojom::SCTAuditingMode::kHashdance}) {
    SCOPED_TRACE(testing::Message() << "Mode: " << static_cast<int>(mode));
    base::HistogramTester histograms;
    handler_->SetMode(mode);
    handler_->MaybeEnqueueReport(host_port_pair, chain_.get(), sct_list);
    auto* pending_reporters = handler_->GetPendingReportersForTesting();
    EXPECT_EQ(pending_reporters->size(),
              mode == mojom::SCTAuditingMode::kHashdance ? 0u : 1u);

    // The hashdance request should record a count for DroppedDueToLogNotFound.
    histograms.ExpectUniqueSample(
        "Security.SCTAuditing.OptOut.DroppedDueToLogNotFound", true,
        mode == mojom::SCTAuditingMode::kHashdance ? 1 : 0);

    // Reset by clearing all pending reports and cache entries.
    base::RunLoop run_loop;
    handler_->ClearPendingReports(run_loop.QuitClosure());
    network_service_->sct_auditing_cache()->ClearCache();
    run_loop.Run();
  }
}

// Regression test for crbug.com/1344881
// Writes a pre-existing persisted reporter, starts up the SCTAuditingHandler,
// waits for the Reporter to be created from the persisted data, and then
// clears the pending reporters. (Prior to the fix for crbug.com/1344881, it was
// possible for the ImportantFileWriter `after_write_callback` to run on the
// background sequence in some circumstances, causing thread safety violations
// when dereferencing the WeakPtr. This test explicitly covers the case where
// the `after_write_callback` code path is exercised.)
TEST_F(SCTAuditingHandlerTest, ClearPendingReports) {
  // Set up previously persisted data on disk:
  // - Default-initialized net::HashValue(net::HASH_VALUE_SHA256)
  // - Empty SCTClientReport for origin "example.test:443".
  // - A simple BackoffEntry.
  std::string persisted_report =
      R"(
        [{
          "reporter_key":
            "sha256/qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqo=",
          "report": "EhUKExIRCgxleGFtcGxlLnRlc3QQuwM=",
          "backoff_entry": [2,0,"30000000","11644578625551798"]
        }]
      )";
  ASSERT_TRUE(base::WriteFile(persistence_path_, persisted_report));

  mojo::PendingRemote<network::mojom::URLLoaderFactory> factory_remote;
  url_loader_factory_.Clone(factory_remote.InitWithNewPipeAndPassReceiver());

  SCTAuditingHandler handler(network_context_.get(), persistence_path_);
  handler.SetMode(mojom::SCTAuditingMode::kEnhancedSafeBrowsingReporting);
  handler.SetURLLoaderFactoryForTesting(std::move(factory_remote));

  auto* file_writer = handler.GetFileWriterForTesting();
  ASSERT_TRUE(file_writer);

  WaitForRequests(1u);

  EXPECT_EQ(handler.GetPendingReportersForTesting()->size(), 1u);
  EXPECT_EQ(1, url_loader_factory_.NumPending());

  // Clear pending reports (with persistence set up and data on disk) to
  // exercise the full clearing and callbacks code paths.
  base::RunLoop run_loop;
  handler.ClearPendingReports(run_loop.QuitClosure());
  run_loop.Run();

  // Check that the pending reporter was deleted.
  EXPECT_TRUE(handler.GetPendingReportersForTesting()->empty());

  // Check that the Reporter is no longer in the file.
  EXPECT_FALSE(FileContentsHasString(
      "sha256/qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqo="));
}

// Test that the "counted_towards_report_limit" flag is correctly reapplied when
// deserialize a persisted reporter. See crbug.com/1348313.
TEST_F(SCTAuditingHandlerTest, PersistedDataWithReportAlreadyCounted) {
  // Set up previously persisted data on disk:
  // - Default-initialized net::HashValue(net::HASH_VALUE_SHA256)
  // - Empty SCTClientReport for origin "example.test:443".
  // - A simple BackoffEntry.
  // - A simple SCTHashdanceMetadata value.
  // - The "already counted toward report limit" flag set to `true`.
  std::string persisted_report =
      R"(
        [{
          "reporter_key":
            "sha256/qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqo=",
          "report": "EhUKExIRCgxleGFtcGxlLnRlc3QQuwM=",
          "backoff_entry": [2,0,"30000000","11644578625551798"],
          "sct_metadata": {
            "leaf_hash": "ZmFrZS1sZWFmLWhhc2g=",
            "issued": "1659045681000000",
            "log_id": "ZmFrZS1sb2ctaWQ=",
            "log_mmd": "86400000000",
            "cert_expiry": "1661724081000000"
          },
          "counted_towards_report_limit": true
        }]
      )";
  ASSERT_TRUE(base::WriteFile(persistence_path_, persisted_report));

  mojo::PendingRemote<network::mojom::URLLoaderFactory> factory_remote;
  url_loader_factory_.Clone(factory_remote.InitWithNewPipeAndPassReceiver());

  SCTAuditingHandler handler(network_context_.get(), persistence_path_);
  handler.SetMode(mojom::SCTAuditingMode::kHashdance);
  handler.SetURLLoaderFactoryForTesting(std::move(factory_remote));

  // Wait for a lookup query request to be sent to ensure the persisted report
  // has been deserialized and a new SCTAuditingReporter created.
  WaitForRequests(1u);

  auto* pending_reporters = handler.GetPendingReportersForTesting();
  ASSERT_EQ(pending_reporters->size(), 1u);
  // Reporter should have the `counted_toward_report_limit` flag set to `true`.
  for (const auto& reporter : *pending_reporters) {
    EXPECT_TRUE(reporter.second->counted_towards_report_limit());
  }
}

// Test that when a persisted reporter is deserialized that does not have the
// "counted_towards_report_limit" flag set, it gets defaulted to `false` in the
// newly created SCTAuditingReporter. (This covers the case for existing
// serialized data from versions before the flag was added.)
// See crbug.com/1348313.
TEST_F(SCTAuditingHandlerTest, PersistedDataWithoutReportAlreadyCounted) {
  // Set up previously persisted data on disk:
  // - Default-initialized net::HashValue(net::HASH_VALUE_SHA256)
  // - Empty SCTClientReport for origin "example.test:443".
  // - A simple BackoffEntry.
  // - A simple SCTHashdanceMetadata value.
  // - The "already counted toward report limit" not set.
  std::string persisted_report =
      R"(
        [{
          "reporter_key":
            "sha256/qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqo=",
          "report": "EhUKExIRCgxleGFtcGxlLnRlc3QQuwM=",
          "backoff_entry": [2,0,"30000000","11644578625551798"],
          "sct_metadata": {
            "leaf_hash": "ZmFrZS1sZWFmLWhhc2g=",
            "issued": "1659045681000000",
            "log_id": "ZmFrZS1sb2ctaWQ=",
            "log_mmd": "86400000000",
            "cert_expiry": "1661724081000000"
          }
        }]
      )";
  ASSERT_TRUE(base::WriteFile(persistence_path_, persisted_report));

  mojo::PendingRemote<network::mojom::URLLoaderFactory> factory_remote;
  url_loader_factory_.Clone(factory_remote.InitWithNewPipeAndPassReceiver());

  SCTAuditingHandler handler(network_context_.get(), persistence_path_);
  handler.SetMode(mojom::SCTAuditingMode::kHashdance);
  handler.SetURLLoaderFactoryForTesting(std::move(factory_remote));

  // Wait for a lookup query request to be sent to ensure the persisted report
  // has been deserialized and a new SCTAuditingReporter created.
  WaitForRequests(1u);

  auto* pending_reporters = handler.GetPendingReportersForTesting();
  ASSERT_EQ(pending_reporters->size(), 1u);
  // Reporter should have the `counted_toward_report_limit` flag set to `false`.
  for (const auto& reporter : *pending_reporters) {
    EXPECT_FALSE(reporter.second->counted_towards_report_limit());
  }
}

}  // namespace

}  // namespace network
