// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/sct_auditing/sct_auditing_cache.h"

#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
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
#include "services/network/public/proto/sct_audit_report.pb.h"
#include "services/network/sct_auditing/sct_auditing_handler.h"
#include "services/network/sct_auditing/sct_auditing_reporter.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/utility/utility.h"
#include "third_party/boringssl/src/include/openssl/pool.h"
#include "third_party/boringssl/src/include/openssl/sha.h"

namespace network {

namespace {

class SCTAuditingCacheTest : public testing::Test {
 public:
  SCTAuditingCacheTest() = default;
  ~SCTAuditingCacheTest() override = default;

  SCTAuditingCacheTest(const SCTAuditingCacheTest&) = delete;
  SCTAuditingCacheTest& operator=(const SCTAuditingCacheTest&) = delete;

  void SetUp() override {
    chain_ =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
    ASSERT_TRUE(chain_.get());
  }

 protected:
  // Initializes the configuration for the SCTAuditingCache to defaults.
  void InitSCTAuditing(SCTAuditingCache* cache, double sampling_rate = 1.0) {
    mojom::SCTAuditingConfigurationPtr configuration(std::in_place);
    configuration->sampling_rate = sampling_rate;
    configuration->traffic_annotation =
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
    cache->Configure(std::move(configuration));
  }

  // Use MOCK_TIME so tests (particularly those involving retry and backoff) can
  // use TaskEnvironment::FastForwardUntilNoTasksRemain() and FastForwardBy().
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

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
net::HashValue ComputeCacheKey(
    net::SignedCertificateTimestampAndStatusList sct_list) {
  net::HashValue cache_key(net::HASH_VALUE_SHA256);
  SHA256_CTX ctx;
  SHA256_Init(&ctx);
  std::string encoded_sct;
  net::ct::EncodeSignedCertificateTimestamp(sct_list.at(0).sct, &encoded_sct);
  SHA256_Update(&ctx, encoded_sct.data(), encoded_sct.size());
  SHA256_Final(reinterpret_cast<uint8_t*>(cache_key.data()), &ctx);
  return cache_key;
}

}  // namespace

// Test that inserting and retrieving a report works.
TEST_F(SCTAuditingCacheTest, InsertAndRetrieveReport) {
  SCTAuditingCache cache(10);
  InitSCTAuditing(&cache);

  const net::HostPortPair host_port_pair("example.com", 443);
  net::SignedCertificateTimestampAndStatusList sct_list;
  MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                       "extensions1", "signature1", base::Time::Now(),
                       net::ct::SCT_STATUS_OK, &sct_list);
  EXPECT_TRUE(
      cache.MaybeGenerateReportEntry(host_port_pair, chain_.get(), sct_list));
  EXPECT_EQ(1u, cache.GetCacheForTesting()->size());
}

// Tests that old entries are evicted when the dedupe cache is full.
TEST_F(SCTAuditingCacheTest, EvictLRUAfterCacheFull) {
  SCTAuditingCache cache(2);
  InitSCTAuditing(&cache);

  const net::HostPortPair host_port_pair1("example1.com", 443);
  const net::HostPortPair host_port_pair2("example2.com", 443);
  const net::HostPortPair host_port_pair3("example3.com", 443);

  net::HashValue first_key(net::HASH_VALUE_SHA256);
  {
    net::SignedCertificateTimestampAndStatusList sct_list;
    MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                         "extensions1", "signature1", base::Time::Now(),
                         net::ct::SCT_STATUS_OK, &sct_list);
    ASSERT_TRUE(cache.MaybeGenerateReportEntry(host_port_pair1, chain_.get(),
                                               sct_list));
    ASSERT_EQ(1u, cache.GetCacheForTesting()->size());

    // Save the initial cache key for later inspection.
    first_key = ComputeCacheKey(sct_list);
  }

  {
    net::SignedCertificateTimestampAndStatusList sct_list;
    MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                         "extensions1", "signature2", base::Time::Now(),
                         net::ct::SCT_STATUS_OK, &sct_list);
    ASSERT_TRUE(cache.MaybeGenerateReportEntry(host_port_pair2, chain_.get(),
                                               sct_list));
    ASSERT_EQ(2u, cache.GetCacheForTesting()->size());
  }

  // Cache is now full. Add another entry to trigger eviction.
  {
    net::SignedCertificateTimestampAndStatusList sct_list;
    MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                         "extensions1", "signature3", base::Time::Now(),
                         net::ct::SCT_STATUS_OK, &sct_list);
    ASSERT_TRUE(cache.MaybeGenerateReportEntry(host_port_pair3, chain_.get(),
                                               sct_list));
    ASSERT_EQ(2u, cache.GetCacheForTesting()->size());
    // The key for the first entry should not be in the cache anymore.
    EXPECT_EQ(cache.GetCacheForTesting()->Get(first_key),
              cache.GetCacheForTesting()->end());
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
  ASSERT_TRUE(
      cache.MaybeGenerateReportEntry(host_port_pair1, chain_.get(), sct_list));

  ASSERT_EQ(1u, cache.GetCacheForTesting()->size());

  // Generating a report entry for the same SCTs won't cause a new report to be
  // generated (even if the connection origin is different).
  ASSERT_FALSE(
      cache.MaybeGenerateReportEntry(host_port_pair2, chain_.get(), sct_list));
  EXPECT_EQ(1u, cache.GetCacheForTesting()->size());
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
  cache.MaybeGenerateReportEntry(host_port_pair1, chain_.get(), sct_list1);

  net::SignedCertificateTimestampAndStatusList sct_list2;
  MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                       "extensions2", "signature2", base::Time::Now(),
                       net::ct::SCT_STATUS_OK, &sct_list2);
  cache.MaybeGenerateReportEntry(host_port_pair2, chain_.get(), sct_list2);

  EXPECT_EQ(2u, cache.GetCacheForTesting()->size());

  // Try to enqueue the report for "example1.com" again. It should be deduped.
  cache.MaybeGenerateReportEntry(host_port_pair1, chain_.get(), sct_list1);
  EXPECT_EQ(2u, cache.GetCacheForTesting()->size());

  // If we enqueue a new report causing the cache size limit to be exceeded,
  // "example1.com" should be the most-recent due to getting updated during
  // deduping, and "example2.com" should get evicted instead.
  net::SignedCertificateTimestampAndStatusList sct_list3;
  MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                       "extensions3", "signature3", base::Time::Now(),
                       net::ct::SCT_STATUS_OK, &sct_list3);
  cache.MaybeGenerateReportEntry(host_port_pair3, chain_.get(), sct_list3);

  EXPECT_EQ(2u, cache.GetCacheForTesting()->size());

  net::HashValue evicted_key = ComputeCacheKey(sct_list2);
  EXPECT_EQ(cache.GetCacheForTesting()->Get(evicted_key),
            cache.GetCacheForTesting()->end());
}

TEST_F(SCTAuditingCacheTest, ReportsCachedButNotSentWhenSamplingIsZero) {
  SCTAuditingCache cache(2);
  InitSCTAuditing(&cache, /*sampling_rate=*/0);

  // Generate a report.
  const net::HostPortPair host_port_pair("example.com", 443);
  net::SignedCertificateTimestampAndStatusList sct_list;
  MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                       "extensions", "signature", base::Time::Now(),
                       net::ct::SCT_STATUS_OK, &sct_list);
  // Check that no report is returned.
  EXPECT_FALSE(
      cache.MaybeGenerateReportEntry(host_port_pair, chain_.get(), sct_list));

  // Check that there is one entry in the cache.
  EXPECT_EQ(1u, cache.GetCacheForTesting()->size());
}

// Tests that cache size high water mark metrics are correctly logged.
TEST_F(SCTAuditingCacheTest, HighWaterMarkMetrics) {
  base::HistogramTester histograms;
  SCTAuditingCache cache(2);
  InitSCTAuditing(&cache);

  const net::HostPortPair host_port_pair1("example1.com", 443);
  const net::HostPortPair host_port_pair2("example2.com", 443);

  // Fill the cache with two reports.
  net::SignedCertificateTimestampAndStatusList sct_list1;
  MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                       "extensions1", "signature1", base::Time::Now(),
                       net::ct::SCT_STATUS_OK, &sct_list1);
  EXPECT_TRUE(
      cache.MaybeGenerateReportEntry(host_port_pair1, chain_.get(), sct_list1));

  net::SignedCertificateTimestampAndStatusList sct_list2;
  MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                       "extensions2", "signature2", base::Time::Now(),
                       net::ct::SCT_STATUS_OK, &sct_list2);
  EXPECT_TRUE(
      cache.MaybeGenerateReportEntry(host_port_pair2, chain_.get(), sct_list2));

  EXPECT_EQ(2u, cache.GetCacheForTesting()->size());

  // High-water-mark metrics are recorded once an hour.
  task_environment_.FastForwardBy(base::Hours(1));

  // The bucket for a HWM of 2 should have a single sample.
  histograms.ExpectUniqueSample("Security.SCTAuditing.OptIn.DedupeCacheHWM", 2,
                                1);
}

// Tests that generating a report causes its size to be logged. Trying to log
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
  std::optional<SCTAuditingCache::ReportEntry> entry =
      cache.MaybeGenerateReportEntry(host_port_pair, chain_.get(), sct_list);
  ASSERT_TRUE(entry);

  // Get the size of the pending report and test that it is correctly logged.
  size_t report_size = entry->report->ByteSizeLong();
  ASSERT_GT(report_size, 0u);
  histograms.ExpectUniqueSample("Security.SCTAuditing.OptIn.ReportSize",
                                report_size, 1);

  // Retry enqueueing the same report which will be deduplicated.
  EXPECT_FALSE(
      cache.MaybeGenerateReportEntry(host_port_pair, chain_.get(), sct_list));

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
  InitSCTAuditing(&cache, /*sampling_rate=*/0);

  const net::HostPortPair host_port_pair("example.com", 443);
  net::SignedCertificateTimestampAndStatusList sct_list;
  MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                       "extensions", "signature", base::Time::Now(),
                       net::ct::SCT_STATUS_OK, &sct_list);
  EXPECT_FALSE(
      cache.MaybeGenerateReportEntry(host_port_pair, chain_.get(), sct_list));

  histograms.ExpectUniqueSample("Security.SCTAuditing.OptIn.ReportSampled",
                                false, 1);
  histograms.ExpectTotalCount("Security.SCTAuditing.OptIn.ReportSize", 0);
  histograms.ExpectBucketCount("Security.SCTAuditing.OptIn.ReportDeduplicated",
                               false, 1);
}

}  // namespace network
