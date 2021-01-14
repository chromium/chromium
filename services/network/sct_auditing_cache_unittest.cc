// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/sct_auditing_cache.h"

#include "base/feature_list.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "net/base/host_port_pair.h"
#include "net/cert/ct_serialization.h"
#include "net/cert/sct_status_flags.h"
#include "net/cert/signed_certificate_timestamp.h"
#include "net/cert/signed_certificate_timestamp_and_status.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/proto/sct_audit_report.pb.h"
#include "services/network/test/fake_test_cert_verifier_params_factory.h"
#include "services/network/test/test_network_context_client.h"

#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {

class SCTAuditingCacheTest : public testing::Test {
 public:
  SCTAuditingCacheTest()
      : network_service_(NetworkService::CreateForTesting()) {}
  ~SCTAuditingCacheTest() override = default;

  SCTAuditingCacheTest(const SCTAuditingCacheTest&) = delete;
  SCTAuditingCacheTest& operator=(const SCTAuditingCacheTest&) = delete;

  void SetUp() override {
    InitNetworkContext();
    network_context_->SetIsSCTAuditingEnabledForTesting(true);
    chain_ =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
    ASSERT_TRUE(chain_.get());
  }

 protected:
  void InitNetworkContext() {
    mojom::NetworkContextParamsPtr params = mojom::NetworkContextParams::New();
    // Use a dummy CertVerifier that always passes cert verification, since
    // these unittests don't need to test CertVerifier behavior.
    params->cert_verifier_params =
        FakeTestCertVerifierParamsFactory::GetCertVerifierParams();

    network_context_ = std::make_unique<NetworkContext>(
        network_service_.get(),
        network_context_remote_.BindNewPipeAndPassReceiver(),
        std::move(params));

    // A NetworkContextClient is needed for embedder notifications to work.
    mojo::PendingRemote<network::mojom::NetworkContextClient>
        network_context_client_remote;
    network_context_client_ =
        std::make_unique<network::TestNetworkContextClient>(
            network_context_client_remote.InitWithNewPipeAndPassReceiver());
    network_context_->SetClient(std::move(network_context_client_remote));
  }

  // Initializes the configuration for the SCTAuditingCache to defaults and
  // sets up the URLLoaderFactory. Individual tests can directly call the set_*
  // methods to tweak the configuration.
  void InitSCTAuditing(SCTAuditingCache* cache) {
    cache->set_enabled(true);
    cache->set_sampling_rate(1.0);
    cache->set_report_uri(GURL("https://example.test"));
    cache->set_traffic_annotation(
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

    url_loader_factory_ = std::make_unique<TestURLLoaderFactory>();
    mojo::PendingRemote<network::mojom::URLLoaderFactory> factory_client;
    url_loader_factory_->Clone(factory_client.InitWithNewPipeAndPassReceiver());
    cache->set_url_loader_factory(std::move(factory_client));
  }

  // Getter for TestURLLoaderFactory to allow tests to specify responses.
  TestURLLoaderFactory* url_loader_factory() {
    return url_loader_factory_.get();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  std::unique_ptr<NetworkService> network_service_;
  std::unique_ptr<NetworkContext> network_context_;
  std::unique_ptr<network::mojom::NetworkContextClient> network_context_client_;
  std::unique_ptr<TestURLLoaderFactory> url_loader_factory_;

  scoped_refptr<net::X509Certificate> chain_;

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

  // The particular value of the log ID doesn't matter; it just has to be the
  // correct length.
  const unsigned char kTestLogId[] = {
      0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
      0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
      0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01};
  const std::string log_id(reinterpret_cast<const char*>(kTestLogId),
                           sizeof(kTestLogId));
  sct->log_id = log_id;

  sct->extensions = extensions;
  sct->timestamp = timestamp;
  sct->signature.signature_data = signature_data;
  sct->origin = origin;
  sct_list->push_back(net::SignedCertificateTimestampAndStatus(sct, status));
}

}  // namespace

// Test that if auditing is disabled on the NetworkContext, no reports are
// cached.
TEST_F(SCTAuditingCacheTest, NoReportsCachedWhenAuditingDisabled) {
  SCTAuditingCache cache(10);
  InitSCTAuditing(&cache);

  network_context_->SetIsSCTAuditingEnabledForTesting(false);

  const net::HostPortPair host_port_pair("example.com", 443);
  net::SignedCertificateTimestampAndStatusList sct_list;
  MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                       "extensions1", "signature1", base::Time::Now(),
                       net::ct::SCT_STATUS_OK, &sct_list);
  cache.MaybeEnqueueReport(network_context_.get(), host_port_pair, chain_.get(),
                           sct_list);

  ASSERT_EQ(0u, cache.GetCacheForTesting()->size());
}

// Test that inserting and retrieving a report works.
TEST_F(SCTAuditingCacheTest, InsertAndRetrieveReport) {
  SCTAuditingCache cache(10);
  InitSCTAuditing(&cache);

  const net::HostPortPair host_port_pair("example.com", 443);
  net::SignedCertificateTimestampAndStatusList sct_list;
  MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                       "extensions1", "signature1", base::Time::Now(),
                       net::ct::SCT_STATUS_OK, &sct_list);
  cache.MaybeEnqueueReport(network_context_.get(), host_port_pair, chain_.get(),
                           sct_list);

  ASSERT_EQ(1u, cache.GetCacheForTesting()->size());
}

// Tests that old entries are evicted when the cache is full.
TEST_F(SCTAuditingCacheTest, EvictLRUAfterCacheFull) {
  SCTAuditingCache cache(2);
  InitSCTAuditing(&cache);

  const net::HostPortPair host_port_pair1("example1.com", 443);
  const net::HostPortPair host_port_pair2("example2.com", 443);
  const net::HostPortPair host_port_pair3("example3.com", 443);

  {
    net::SignedCertificateTimestampAndStatusList sct_list;
    MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                         "extensions1", "signature1", base::Time::Now(),
                         net::ct::SCT_STATUS_OK, &sct_list);
    cache.MaybeEnqueueReport(network_context_.get(), host_port_pair1,
                             chain_.get(), sct_list);
    ASSERT_EQ(1u, cache.GetCacheForTesting()->size());
  }

  {
    net::SignedCertificateTimestampAndStatusList sct_list;
    MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                         "extensions1", "signature2", base::Time::Now(),
                         net::ct::SCT_STATUS_OK, &sct_list);
    cache.MaybeEnqueueReport(network_context_.get(), host_port_pair2,
                             chain_.get(), sct_list);
    ASSERT_EQ(2u, cache.GetCacheForTesting()->size());
  }

  // Cache is now full, so the first entry (for "example1.com") should no longer
  // be in the cache after inserting a third entry.
  {
    net::SignedCertificateTimestampAndStatusList sct_list;
    MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                         "extensions1", "signature3", base::Time::Now(),
                         net::ct::SCT_STATUS_OK, &sct_list);
    cache.MaybeEnqueueReport(network_context_.get(), host_port_pair3,
                             chain_.get(), sct_list);
    ASSERT_EQ(2u, cache.GetCacheForTesting()->size());
    for (const auto& entry : *cache.GetCacheForTesting()) {
      ASSERT_NE(
          "example1.com",
          entry.second->certificate_report(0).context().origin().hostname());
    }
  }
}

// Tests that a new report gets dropped if the same SCTs are already in the
// cache.
TEST_F(SCTAuditingCacheTest, ReportWithSameSCTsDeduplicated) {
  SCTAuditingCache cache(10);
  InitSCTAuditing(&cache);

  const net::HostPortPair host_port_pair1("example.com", 443);
  const net::HostPortPair host_port_pair2("example.org", 443);

  net::SignedCertificateTimestampAndStatusList sct_list;
  MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                       "extensions1", "signature1", base::Time::Now(),
                       net::ct::SCT_STATUS_OK, &sct_list);
  cache.MaybeEnqueueReport(network_context_.get(), host_port_pair1,
                           chain_.get(), sct_list);

  ASSERT_EQ(1u, cache.GetCacheForTesting()->size());

  // Enqueuing the same SCTs won't cause a new report to be added to the queue
  // (even if the connection origin is different).
  cache.MaybeEnqueueReport(network_context_.get(), host_port_pair2,
                           chain_.get(), sct_list);
  ASSERT_EQ(1u, cache.GetCacheForTesting()->size());
}

// When a report gets deduplicated, the existing entry should have its last-seen
// time bumped up.
TEST_F(SCTAuditingCacheTest, DeduplicationUpdatesLastSeenTime) {
  SCTAuditingCache cache(2);
  InitSCTAuditing(&cache);

  const net::HostPortPair host_port_pair1("example1.com", 443);
  const net::HostPortPair host_port_pair2("example2.com", 443);
  const net::HostPortPair host_port_pair3("example3.com", 443);

  // Fill the cache with two reports.
  net::SignedCertificateTimestampAndStatusList sct_list1;
  MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                       "extensions1", "signature1", base::Time::Now(),
                       net::ct::SCT_STATUS_OK, &sct_list1);
  cache.MaybeEnqueueReport(network_context_.get(), host_port_pair1,
                           chain_.get(), sct_list1);

  net::SignedCertificateTimestampAndStatusList sct_list2;
  MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                       "extensions2", "signature2", base::Time::Now(),
                       net::ct::SCT_STATUS_OK, &sct_list2);
  cache.MaybeEnqueueReport(network_context_.get(), host_port_pair2,
                           chain_.get(), sct_list2);

  EXPECT_EQ(2u, cache.GetCacheForTesting()->size());

  // Try to enqueue the report for "example1.com" again. It should be deduped.
  cache.MaybeEnqueueReport(network_context_.get(), host_port_pair1,
                           chain_.get(), sct_list1);
  EXPECT_EQ(2u, cache.GetCacheForTesting()->size());

  // If we enqueue a new report causing the cache size limit to be exceeded,
  // "example1.com" should be the most-recent due to getting updated during
  // deduping, and "example2.com" should get evicted instead.
  net::SignedCertificateTimestampAndStatusList sct_list3;
  MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                       "extensions3", "signature3", base::Time::Now(),
                       net::ct::SCT_STATUS_OK, &sct_list3);
  cache.MaybeEnqueueReport(network_context_.get(), host_port_pair3,
                           chain_.get(), sct_list3);

  EXPECT_EQ(2u, cache.GetCacheForTesting()->size());
  for (const auto& entry : *cache.GetCacheForTesting()) {
    ASSERT_NE(
        "example2.com",
        entry.second->certificate_report(0).context().origin().hostname());
  }
}

TEST_F(SCTAuditingCacheTest, NoReportsCachedWhenCacheDisabled) {
  SCTAuditingCache cache(2);
  InitSCTAuditing(&cache);
  cache.set_enabled(false);

  // Try to enqueue a report.
  const net::HostPortPair host_port_pair("example.com", 443);
  net::SignedCertificateTimestampAndStatusList sct_list;
  MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                       "extensions", "signature", base::Time::Now(),
                       net::ct::SCT_STATUS_OK, &sct_list);
  cache.MaybeEnqueueReport(network_context_.get(), host_port_pair, chain_.get(),
                           sct_list);

  // Check that there are no entries in the cache.
  EXPECT_EQ(0u, cache.GetCacheForTesting()->size());
}

TEST_F(SCTAuditingCacheTest, ReportsCachedButNotSentWhenSamplingIsZero) {
  SCTAuditingCache cache(2);
  InitSCTAuditing(&cache);
  cache.set_sampling_rate(0);

  // Enqueue a report.
  const net::HostPortPair host_port_pair("example.com", 443);
  net::SignedCertificateTimestampAndStatusList sct_list;
  MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                       "extensions", "signature", base::Time::Now(),
                       net::ct::SCT_STATUS_OK, &sct_list);
  cache.MaybeEnqueueReport(network_context_.get(), host_port_pair, chain_.get(),
                           sct_list);

  // Check that there is one entry in the cache.
  EXPECT_EQ(1u, cache.GetCacheForTesting()->size());

  // Check that there are no pending reports.
  EXPECT_EQ(0, url_loader_factory()->NumPending());
}

// Tests that when a new report is sampled, it will be sent to the server.
// TODO(cthomp): Allow tracking success/failure of the report being sent. One
// way would be to have OnSuccess/OnError handlers be defined by an
// SCTAuditingReportingDelegate installed on the cache.
TEST_F(SCTAuditingCacheTest, ReportsSentWithServerOK) {
  SCTAuditingCache cache(2);
  InitSCTAuditing(&cache);

  // Enqueue a report which will trigger a send.
  const net::HostPortPair host_port_pair("example.com", 443);
  net::SignedCertificateTimestampAndStatusList sct_list;
  MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                       "extensions", "signature", base::Time::Now(),
                       net::ct::SCT_STATUS_OK, &sct_list);
  cache.MaybeEnqueueReport(network_context_.get(), host_port_pair, chain_.get(),
                           sct_list);

  // Check that there is one pending report.
  EXPECT_EQ(1, url_loader_factory()->NumPending());

  // Simulate the server returning 200 OK to the report request.
  url_loader_factory()->AddResponse("https://example.test",
                                    /*content=*/"",
                                    /*status=*/net::HTTP_OK);
  task_environment_.RunUntilIdle();

  EXPECT_EQ(0, url_loader_factory()->NumPending());

  // Check that the report has been cleared in the cache as it has been
  // successfully sent.
  for (const auto& entry : *cache.GetCacheForTesting()) {
    EXPECT_FALSE(entry.second);
  }
}

// Tests when the report server returns an HTTP error code.
// TODO(cthomp): Check that the cache treats the send as a failure.
TEST_F(SCTAuditingCacheTest, ReportSentWithServerError) {
  SCTAuditingCache cache(2);
  InitSCTAuditing(&cache);

  // Enqueue a report which will trigger a send.
  const net::HostPortPair host_port_pair("example.com", 443);
  net::SignedCertificateTimestampAndStatusList sct_list;
  MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                       "extensions", "signature", base::Time::Now(),
                       net::ct::SCT_STATUS_OK, &sct_list);
  cache.MaybeEnqueueReport(network_context_.get(), host_port_pair, chain_.get(),
                           sct_list);

  // Check that there is one pending report.
  EXPECT_EQ(1, url_loader_factory()->NumPending());

  // Simulate the server returning 429 TOO MANY REQUEST to the report request.
  url_loader_factory()->AddResponse("https://example.test",
                                    /*content=*/"",
                                    /*status=*/net::HTTP_TOO_MANY_REQUESTS);
  task_environment_.RunUntilIdle();

  EXPECT_EQ(0, url_loader_factory()->NumPending());

  // Check that the report is still stored in the cache as it has not succeeded.
  for (const auto& entry : *cache.GetCacheForTesting()) {
    EXPECT_TRUE(entry.second);
  }
}

// Tests that cache size high water mark metrics are correctly logged.
TEST_F(SCTAuditingCacheTest, HighWaterMarkMetrics) {
  base::HistogramTester histograms;
  // Create a cache so we can trigger destruction when it goes out of scope,
  // which is when HWM metrics are logged.
  {
    SCTAuditingCache cache(5);
    InitSCTAuditing(&cache);

    const net::HostPortPair host_port_pair1("example1.com", 443);
    const net::HostPortPair host_port_pair2("example2.com", 443);

    // Fill the cache with two reports.
    net::SignedCertificateTimestampAndStatusList sct_list1;
    MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                         "extensions1", "signature1", base::Time::Now(),
                         net::ct::SCT_STATUS_OK, &sct_list1);
    cache.MaybeEnqueueReport(network_context_.get(), host_port_pair1,
                             chain_.get(), sct_list1);

    net::SignedCertificateTimestampAndStatusList sct_list2;
    MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                         "extensions2", "signature2", base::Time::Now(),
                         net::ct::SCT_STATUS_OK, &sct_list2);
    cache.MaybeEnqueueReport(network_context_.get(), host_port_pair2,
                             chain_.get(), sct_list2);

    EXPECT_EQ(2u, cache.GetCacheForTesting()->size());
  }

  // The bucket for a HWM of 2 should have a single sample as there were two
  // items in the cache when it was destroyed.
  histograms.ExpectUniqueSample("Security.SCTAuditing.OptIn.CacheHWM", 2, 1);
}

// Tests that enqueueing a report causes its size to be logged. Trying to log
// the same SCTs a second time will cause the deduplication to be logged instead
// of logging the report size a second time.
TEST_F(SCTAuditingCacheTest, ReportSizeMetrics) {
  SCTAuditingCache cache(10);
  InitSCTAuditing(&cache);

  base::HistogramTester histograms;

  const net::HostPortPair host_port_pair("example.com", 443);
  net::SignedCertificateTimestampAndStatusList sct_list;
  MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                       "extensions", "signature", base::Time::Now(),
                       net::ct::SCT_STATUS_OK, &sct_list);
  cache.MaybeEnqueueReport(network_context_.get(), host_port_pair, chain_.get(),
                           sct_list);

  // Get the size of the enqueued report and test that it is correctly logged.
  size_t report_size =
      cache.GetCacheForTesting()->begin()->second->ByteSizeLong();
  ASSERT_GT(report_size, 0u);
  histograms.ExpectUniqueSample("Security.SCTAuditing.OptIn.ReportSize",
                                report_size, 1);

  // Retry enqueueing the same report which will be deduplicated.
  cache.MaybeEnqueueReport(network_context_.get(), host_port_pair, chain_.get(),
                           sct_list);

  histograms.ExpectTotalCount("Security.SCTAuditing.OptIn.ReportSampled", 1);
  histograms.ExpectTotalCount("Security.SCTAuditing.OptIn.ReportSize", 1);
  histograms.ExpectBucketCount("Security.SCTAuditing.OptIn.ReportDeduplicated",
                               true, 1);
}

// Test that metrics for when reports are dropped due to sampling are correctly
// logged.
TEST_F(SCTAuditingCacheTest, ReportSampleDroppedMetrics) {
  base::HistogramTester histograms;

  SCTAuditingCache cache(10);
  InitSCTAuditing(&cache);
  cache.set_sampling_rate(0);

  const net::HostPortPair host_port_pair("example.com", 443);
  net::SignedCertificateTimestampAndStatusList sct_list;
  MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                       "extensions", "signature", base::Time::Now(),
                       net::ct::SCT_STATUS_OK, &sct_list);
  cache.MaybeEnqueueReport(network_context_.get(), host_port_pair, chain_.get(),
                           sct_list);

  histograms.ExpectUniqueSample("Security.SCTAuditing.OptIn.ReportSampled",
                                false, 1);
  histograms.ExpectTotalCount("Security.SCTAuditing.OptIn.ReportSize", 0);
  histograms.ExpectBucketCount("Security.SCTAuditing.OptIn.ReportDeduplicated",
                               false, 1);
}

// If a report doesn't have any valid SCTs, it should not get added to the cache
// at all.
TEST_F(SCTAuditingCacheTest, ReportNotGeneratedIfNoValidSCTs) {
  SCTAuditingCache cache(10);
  InitSCTAuditing(&cache);

  const net::HostPortPair host_port_pair("example.com", 443);
  net::SignedCertificateTimestampAndStatusList sct_list;
  MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                       "extensions", "signature", base::Time::Now(),
                       net::ct::SCT_STATUS_INVALID_SIGNATURE, &sct_list);
  cache.MaybeEnqueueReport(network_context_.get(), host_port_pair, chain_.get(),
                           sct_list);

  EXPECT_EQ(0u, cache.GetCacheForTesting()->size());
}

// Connections that have a mix of valid and invalid SCTs should only include the
// valid SCTs in the report.
TEST_F(SCTAuditingCacheTest, ReportsOnlyIncludesValidSCTs) {
  SCTAuditingCache cache(10);
  InitSCTAuditing(&cache);

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
  cache.MaybeEnqueueReport(network_context_.get(), host_port_pair, chain_.get(),
                           sct_list);

  // No invalid SCTs should be in any reports in the cache.
  for (const auto& entry : *cache.GetCacheForTesting()) {
    for (auto& sct_and_status :
         entry.second->certificate_report(0).included_sct()) {
      // Decode the SCT and check that only the valid SCT was included.
      base::StringPiece encoded_sct(sct_and_status.serialized_sct());
      scoped_refptr<net::ct::SignedCertificateTimestamp> decoded_sct;
      ASSERT_TRUE(net::ct::DecodeSignedCertificateTimestamp(&encoded_sct,
                                                            &decoded_sct));
      EXPECT_EQ("valid_signature", decoded_sct->signature.signature_data);
    }
  }
}

}  // namespace network
