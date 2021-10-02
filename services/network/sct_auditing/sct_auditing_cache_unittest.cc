// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/sct_auditing/sct_auditing_cache.h"

#include "base/feature_list.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"
#include "net/base/hash_value.h"
#include "net/base/host_port_pair.h"
#include "net/cert/ct_serialization.h"
#include "net/cert/sct_status_flags.h"
#include "net/cert/signed_certificate_timestamp.h"
#include "net/cert/signed_certificate_timestamp_and_status.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/proto/sct_audit_report.pb.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/pool.h"
#include "third_party/boringssl/src/include/openssl/sha.h"
#include "url/gurl.h"

namespace network {

namespace {

class SCTAuditingCacheTest : public testing::Test {
 public:
  SCTAuditingCacheTest() {}
  ~SCTAuditingCacheTest() override = default;

  SCTAuditingCacheTest(const SCTAuditingCacheTest&) = delete;
  SCTAuditingCacheTest& operator=(const SCTAuditingCacheTest&) = delete;

  void SetUp() override {
    chain_ =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
    ASSERT_TRUE(chain_.get());
  }

 protected:
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
    mojo::PendingRemote<network::mojom::URLLoaderFactory> factory_remote;
    url_loader_factory_->Clone(factory_remote.InitWithNewPipeAndPassReceiver());
    cache->set_url_loader_factory(std::move(factory_remote));
  }

  // Getter for TestURLLoaderFactory to allow tests to specify responses.
  TestURLLoaderFactory* url_loader_factory() {
    return url_loader_factory_.get();
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
    url_loader_factory()->SetInterceptor(base::BindLambdaForTesting(
        [&](const network::ResourceRequest& request) {
          ++num_requests_seen_;
          if (run_loop_->running() && num_requests_seen_ >= expected_requests) {
            run_loop_->QuitWhenIdle();
          }
        }));

    run_loop_->Run();
  }

  // Use MOCK_TIME so tests (particularly those involving retry and backoff) can
  // use TaskEnvironment::FastForwardUntilNoTasksRemain() and FastForwardBy().
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<TestURLLoaderFactory> url_loader_factory_;

  scoped_refptr<net::X509Certificate> chain_;

  std::unique_ptr<base::RunLoop> run_loop_;
  size_t num_requests_seen_ = 0;
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

// Computes the cache key from a list of SCTs. This matches how SCTAuditingCache
// computes cache keys internally.
net::SHA256HashValue ComputeCacheKey(
    net::SignedCertificateTimestampAndStatusList sct_list) {
  net::SHA256HashValue cache_key;
  SHA256_CTX ctx;
  SHA256_Init(&ctx);
  std::string encoded_sct;
  net::ct::EncodeSignedCertificateTimestamp(sct_list.at(0).sct, &encoded_sct);
  SHA256_Update(&ctx, encoded_sct.data(), encoded_sct.size());
  SHA256_Final(reinterpret_cast<uint8_t*>(&cache_key), &ctx);
  return cache_key;
}

}  // namespace

// Test that if auditing is disabled, no reports are cached.
TEST_F(SCTAuditingCacheTest, NoReportsCachedWhenAuditingDisabled) {
  SCTAuditingCache cache(10);
  InitSCTAuditing(&cache);
  cache.set_enabled(false);

  const net::HostPortPair host_port_pair("example.com", 443);
  net::SignedCertificateTimestampAndStatusList sct_list;
  MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                       "extensions1", "signature1", base::Time::Now(),
                       net::ct::SCT_STATUS_OK, &sct_list);
  cache.MaybeEnqueueReport(host_port_pair, chain_.get(), sct_list);

  EXPECT_EQ(0u, cache.GetCacheForTesting()->size());
  EXPECT_EQ(0u, cache.GetPendingReportersForTesting()->size());
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
  cache.MaybeEnqueueReport(host_port_pair, chain_.get(), sct_list);

  EXPECT_EQ(1u, cache.GetCacheForTesting()->size());
  EXPECT_EQ(1u, cache.GetPendingReportersForTesting()->size());
}

// Tests that old entries are evicted when the dedupe cache and pending
// reporters set are full.
TEST_F(SCTAuditingCacheTest, EvictLRUAfterCacheFull) {
  SCTAuditingCache cache(2);
  InitSCTAuditing(&cache);

  const net::HostPortPair host_port_pair1("example1.com", 443);
  const net::HostPortPair host_port_pair2("example2.com", 443);
  const net::HostPortPair host_port_pair3("example3.com", 443);

  net::SHA256HashValue first_key;
  {
    net::SignedCertificateTimestampAndStatusList sct_list;
    MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                         "extensions1", "signature1", base::Time::Now(),
                         net::ct::SCT_STATUS_OK, &sct_list);
    cache.MaybeEnqueueReport(host_port_pair1, chain_.get(), sct_list);
    ASSERT_EQ(1u, cache.GetCacheForTesting()->size());
    ASSERT_EQ(1u, cache.GetPendingReportersForTesting()->size());

    // Save the initial cache key for later inspection.
    first_key = ComputeCacheKey(sct_list);
  }

  {
    net::SignedCertificateTimestampAndStatusList sct_list;
    MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                         "extensions1", "signature2", base::Time::Now(),
                         net::ct::SCT_STATUS_OK, &sct_list);
    cache.MaybeEnqueueReport(host_port_pair2, chain_.get(), sct_list);
    ASSERT_EQ(2u, cache.GetCacheForTesting()->size());
    ASSERT_EQ(2u, cache.GetPendingReportersForTesting()->size());
  }

  // Cache is now full. Add another entry to trigger eviction.
  {
    net::SignedCertificateTimestampAndStatusList sct_list;
    MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                         "extensions1", "signature3", base::Time::Now(),
                         net::ct::SCT_STATUS_OK, &sct_list);
    cache.MaybeEnqueueReport(host_port_pair3, chain_.get(), sct_list);
    ASSERT_EQ(2u, cache.GetCacheForTesting()->size());
    ASSERT_EQ(2u, cache.GetPendingReportersForTesting()->size());
    // The key for the first entry should not be in the cache anymore.
    EXPECT_EQ(cache.GetCacheForTesting()->Get(first_key),
              cache.GetCacheForTesting()->end());
    EXPECT_EQ(cache.GetPendingReportersForTesting()->Get(first_key),
              cache.GetPendingReportersForTesting()->end());
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
  cache.MaybeEnqueueReport(host_port_pair1, chain_.get(), sct_list);

  ASSERT_EQ(1u, cache.GetCacheForTesting()->size());

  // Enqueuing the same SCTs won't cause a new report to be added to the queue
  // (even if the connection origin is different).
  cache.MaybeEnqueueReport(host_port_pair2, chain_.get(), sct_list);
  EXPECT_EQ(1u, cache.GetCacheForTesting()->size());
  EXPECT_EQ(1u, cache.GetPendingReportersForTesting()->size());
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
  cache.MaybeEnqueueReport(host_port_pair1, chain_.get(), sct_list1);

  net::SignedCertificateTimestampAndStatusList sct_list2;
  MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                       "extensions2", "signature2", base::Time::Now(),
                       net::ct::SCT_STATUS_OK, &sct_list2);
  cache.MaybeEnqueueReport(host_port_pair2, chain_.get(), sct_list2);

  EXPECT_EQ(2u, cache.GetCacheForTesting()->size());

  // Try to enqueue the report for "example1.com" again. It should be deduped.
  cache.MaybeEnqueueReport(host_port_pair1, chain_.get(), sct_list1);
  EXPECT_EQ(2u, cache.GetCacheForTesting()->size());

  // If we enqueue a new report causing the cache size limit to be exceeded,
  // "example1.com" should be the most-recent due to getting updated during
  // deduping, and "example2.com" should get evicted instead.
  net::SignedCertificateTimestampAndStatusList sct_list3;
  MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                       "extensions3", "signature3", base::Time::Now(),
                       net::ct::SCT_STATUS_OK, &sct_list3);
  cache.MaybeEnqueueReport(host_port_pair3, chain_.get(), sct_list3);

  EXPECT_EQ(2u, cache.GetCacheForTesting()->size());

  net::SHA256HashValue evicted_key = ComputeCacheKey(sct_list2);
  EXPECT_EQ(cache.GetCacheForTesting()->Get(evicted_key),
            cache.GetCacheForTesting()->end());
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
  cache.MaybeEnqueueReport(host_port_pair, chain_.get(), sct_list);

  // Check that there is one entry in the cache.
  EXPECT_EQ(1u, cache.GetCacheForTesting()->size());

  // Check that there are no pending reports.
  EXPECT_EQ(0, url_loader_factory()->NumPending());
  EXPECT_EQ(0u, cache.GetPendingReportersForTesting()->size());
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
  cache.MaybeEnqueueReport(host_port_pair, chain_.get(), sct_list);

  // Check that there is one pending report.
  EXPECT_EQ(1, url_loader_factory()->NumPending());
  EXPECT_EQ(1u, cache.GetPendingReportersForTesting()->size());

  // Simulate the server returning 200 OK to the report request.
  url_loader_factory()->AddResponse("https://example.test",
                                    /*content=*/"",
                                    /*status=*/net::HTTP_OK);

  // No wait currently needed here, as without retry enabled the report is sent
  // synchronously.

  EXPECT_EQ(0, url_loader_factory()->NumPending());

  // Check that the pending reporter was deleted on successful completion.
  EXPECT_TRUE(cache.GetPendingReportersForTesting()->empty());
}

// Tests when the report server returns an HTTP error code.
TEST_F(SCTAuditingCacheTest, ReportSentWithServerError) {
  SCTAuditingCache cache(2);
  InitSCTAuditing(&cache);

  // Enqueue a report which will trigger a send.
  const net::HostPortPair host_port_pair("example.com", 443);
  net::SignedCertificateTimestampAndStatusList sct_list;
  MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                       "extensions", "signature", base::Time::Now(),
                       net::ct::SCT_STATUS_OK, &sct_list);
  cache.MaybeEnqueueReport(host_port_pair, chain_.get(), sct_list);

  // Check that there is one pending report.
  EXPECT_EQ(1, url_loader_factory()->NumPending());

  // Simulate the server returning 429 TOO MANY REQUEST to the report request.
  url_loader_factory()->AddResponse("https://example.test",
                                    /*content=*/"",
                                    /*status=*/net::HTTP_TOO_MANY_REQUESTS);

  // No wait currently needed here, as without retry enabled the report is sent
  // synchronously.

  EXPECT_EQ(0, url_loader_factory()->NumPending());
  // Without retry enabled, the pending reporter should get cleared on error.
  EXPECT_EQ(0u, cache.GetPendingReportersForTesting()->size());
}

// Tests that cache size high water mark metrics are correctly logged.
TEST_F(SCTAuditingCacheTest, HighWaterMarkMetrics) {
  base::HistogramTester histograms;

  SCTAuditingCache cache(5);
  InitSCTAuditing(&cache);

  const net::HostPortPair host_port_pair1("example1.com", 443);
  const net::HostPortPair host_port_pair2("example2.com", 443);

  // Fill the cache with two reports.
  net::SignedCertificateTimestampAndStatusList sct_list1;
  MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                       "extensions1", "signature1", base::Time::Now(),
                       net::ct::SCT_STATUS_OK, &sct_list1);
  cache.MaybeEnqueueReport(host_port_pair1, chain_.get(), sct_list1);

  net::SignedCertificateTimestampAndStatusList sct_list2;
  MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                       "extensions2", "signature2", base::Time::Now(),
                       net::ct::SCT_STATUS_OK, &sct_list2);
  cache.MaybeEnqueueReport(host_port_pair2, chain_.get(), sct_list2);

  EXPECT_EQ(2u, cache.GetCacheForTesting()->size());
  EXPECT_EQ(2u, cache.GetPendingReportersForTesting()->size());

  // High-water-mark metrics are recorded once an hour.
  task_environment_.FastForwardBy(base::Hours(1));

  // The bucket for a HWM of 2 should have a single sample in each of the HWM
  // histograms.
  histograms.ExpectUniqueSample("Security.SCTAuditing.OptIn.DedupeCacheHWM", 2,
                                1);
  histograms.ExpectUniqueSample("Security.SCTAuditing.OptIn.ReportersHWM", 2,
                                1);
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
  cache.MaybeEnqueueReport(host_port_pair, chain_.get(), sct_list);

  // Get the size of the pending report and test that it is correctly logged.
  size_t report_size = cache.GetPendingReportersForTesting()
                           ->begin()
                           ->second->report()
                           ->ByteSizeLong();
  ASSERT_GT(report_size, 0u);
  histograms.ExpectUniqueSample("Security.SCTAuditing.OptIn.ReportSize",
                                report_size, 1);

  // Retry enqueueing the same report which will be deduplicated.
  cache.MaybeEnqueueReport(host_port_pair, chain_.get(), sct_list);

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
  cache.MaybeEnqueueReport(host_port_pair, chain_.get(), sct_list);

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
  cache.MaybeEnqueueReport(host_port_pair, chain_.get(), sct_list);

  EXPECT_EQ(0u, cache.GetCacheForTesting()->size());
  EXPECT_EQ(0u, cache.GetPendingReportersForTesting()->size());
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
  cache.MaybeEnqueueReport(host_port_pair, chain_.get(), sct_list);

  ASSERT_EQ(1u, cache.GetPendingReportersForTesting()->size());

  // No invalid SCTs should be in any of the pending reports.
  for (const auto& reporter : *cache.GetPendingReportersForTesting()) {
    for (auto& sct_and_status :
         reporter.second->report()->certificate_report(0).included_sct()) {
      // Decode the SCT and check that only the valid SCT was included.
      base::StringPiece encoded_sct(sct_and_status.serialized_sct());
      scoped_refptr<net::ct::SignedCertificateTimestamp> decoded_sct;
      ASSERT_TRUE(net::ct::DecodeSignedCertificateTimestamp(&encoded_sct,
                                                            &decoded_sct));
      EXPECT_EQ("valid_signature", decoded_sct->signature.signature_data);
    }
  }
}

// Tests a single retry. The server initially returns an error, but then returns
// OK the second try.
TEST_F(SCTAuditingCacheTest, ReportSucceedsOnSecondTry) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kSCTAuditingRetryAndPersistReports);

  base::HistogramTester histograms;

  SCTAuditingCache cache(2);
  InitSCTAuditing(&cache);

  // Enqueue a report which will trigger a send.
  const net::HostPortPair host_port_pair("example.com", 443);
  net::SignedCertificateTimestampAndStatusList sct_list;
  MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                       "extensions", "signature", base::Time::Now(),
                       net::ct::SCT_STATUS_OK, &sct_list);
  cache.MaybeEnqueueReport(host_port_pair, chain_.get(), sct_list);

  // Check that there is one pending report.
  EXPECT_EQ(1u, cache.GetPendingReportersForTesting()->size());

  // Wait for initial request.
  WaitForRequests(1u);

  // Simulate the server returning 429 TOO MANY REQUEST to the report request.
  // The request must already be pending before calling this.
  url_loader_factory()->SimulateResponseForPendingRequest(
      "https://example.test",
      /*content=*/"",
      /*status=*/net::HTTP_TOO_MANY_REQUESTS);

  EXPECT_EQ(0, url_loader_factory()->NumPending());
  // With retry enabled, the pending reporter should remain on failure.
  EXPECT_EQ(1u, cache.GetPendingReportersForTesting()->size());

  // Simulate the server returning 200 OK to the report request. The request is
  // not yet pending, so set the "default" response. Then when
  // FastForwardUntilNoTasksRemain() is called, the retry will trigger and
  // succeed. This is more robust than manually advancing the mock time due to
  // the jitter specified by the backoff policy and the timeout set on the
  // SimpleURLLoader.
  url_loader_factory()->AddResponse("https://example.test",
                                    /*content=*/"",
                                    /*status=*/net::HTTP_OK);
  // Wait for second request.
  WaitForRequests(2u);

  EXPECT_EQ(0, url_loader_factory()->NumPending());

  // Check that the pending reporter was deleted on successful completion.
  EXPECT_TRUE(cache.GetPendingReportersForTesting()->empty());

  histograms.ExpectUniqueSample(
      "Security.SCTAuditing.OptIn.ReportCompletionStatus",
      SCTAuditingReporter::CompletionStatus::kSuccessAfterRetries, 1);
}

// Tests that after max_tries, the reporter stops and is deleted.
TEST_F(SCTAuditingCacheTest, ExhaustAllRetriesShouldDeleteReporter) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kSCTAuditingRetryAndPersistReports);

  base::HistogramTester histograms;

  SCTAuditingCache cache(2);
  InitSCTAuditing(&cache);

  // Enqueue a report which will trigger a send.
  const net::HostPortPair host_port_pair("example.com", 443);
  net::SignedCertificateTimestampAndStatusList sct_list;
  MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                       "extensions", "signature", base::Time::Now(),
                       net::ct::SCT_STATUS_OK, &sct_list);
  cache.MaybeEnqueueReport(host_port_pair, chain_.get(), sct_list);

  // Check that there is one pending reporter.
  EXPECT_EQ(1u, cache.GetPendingReportersForTesting()->size());

  // Simulate the server returning 429 TOO MANY REQUEST to every request
  url_loader_factory()->AddResponse("https://example.test",
                                    /*content=*/"",
                                    /*status=*/net::HTTP_TOO_MANY_REQUESTS);

  // Wait for initial request + 15 retries.
  WaitForRequests(16u);

  // The reporter should be deleted when it runs out of retries.
  EXPECT_TRUE(cache.GetPendingReportersForTesting()->empty());

  // The Reporter should send 16 requests: 1 initial attempt, and 15 retries
  // (the default max_retries for SCTAuditingReporter).
  EXPECT_EQ(16u, num_requests_seen_);

  histograms.ExpectUniqueSample(
      "Security.SCTAuditing.OptIn.ReportCompletionStatus",
      SCTAuditingReporter::CompletionStatus::kRetriesExhausted, 1);
}

// Tests that report completion metrics are correctly recorded when a report
// succeeds on the first try.
TEST_F(SCTAuditingCacheTest, RetriesEnabledSucceedFirstTryMetrics) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kSCTAuditingRetryAndPersistReports);

  base::HistogramTester histograms;

  SCTAuditingCache cache(2);
  InitSCTAuditing(&cache);

  // Enqueue a report which will trigger a send.
  const net::HostPortPair host_port_pair("example.com", 443);
  net::SignedCertificateTimestampAndStatusList sct_list;
  MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                       "extensions", "signature", base::Time::Now(),
                       net::ct::SCT_STATUS_OK, &sct_list);
  cache.MaybeEnqueueReport(host_port_pair, chain_.get(), sct_list);

  // Wait for the initial request to be pending.
  WaitForRequests(1u);

  EXPECT_EQ(1, url_loader_factory()->NumPending());

  // Simulate the server returning 200 OK to the report request.
  url_loader_factory()->SimulateResponseForPendingRequest(
      "https://example.test",
      /*content=*/"",
      /*status=*/net::HTTP_OK);

  // "Success on first try" should be logged to the histogram.
  histograms.ExpectUniqueSample(
      "Security.SCTAuditing.OptIn.ReportCompletionStatus",
      SCTAuditingReporter::CompletionStatus::kSuccessFirstTry, 1);
}

}  // namespace network
