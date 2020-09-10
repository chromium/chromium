// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/sct_auditing_cache.h"

#include "base/feature_list.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "net/base/host_port_pair.h"
#include "net/cert/sct_status_flags.h"
#include "net/cert/signed_certificate_timestamp.h"
#include "net/cert/signed_certificate_timestamp_and_status.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/proto/sct_audit_report.pb.h"
#include "services/network/test/test_network_context_client.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {

base::test::ScopedFeatureList::FeatureAndParams probability_zero{
    network::features::kSCTAuditing,
    {{network::features::kSCTAuditingSamplingRate.name, "0.0"}}};
base::test::ScopedFeatureList::FeatureAndParams probability_one{
    network::features::kSCTAuditing,
    {{network::features::kSCTAuditingSamplingRate.name, "1.0"}}};

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
    network_context_ = std::make_unique<NetworkContext>(
        network_service_.get(),
        network_context_remote_.BindNewPipeAndPassReceiver(),
        mojom::NetworkContextParams::New());

    // A NetworkContextClient is needed for embedder notifications to work.
    mojo::PendingRemote<network::mojom::NetworkContextClient>
        network_context_client_remote;
    network_context_client_ =
        std::make_unique<network::TestNetworkContextClient>(
            network_context_client_remote.InitWithNewPipeAndPassReceiver());
    network_context_->SetClient(std::move(network_context_client_remote));
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  std::unique_ptr<NetworkService> network_service_;
  std::unique_ptr<NetworkContext> network_context_;
  std::unique_ptr<network::mojom::NetworkContextClient> network_context_client_;

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
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters({probability_one}, {});
  SCTAuditingCache cache(10);

  network_context_->SetIsSCTAuditingEnabledForTesting(false);

  const net::HostPortPair host_port_pair("example.com", 443);
  net::SignedCertificateTimestampAndStatusList sct_list;
  MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                       "extensions1", "signature1", base::Time::Now(),
                       net::ct::SCT_STATUS_LOG_UNKNOWN, &sct_list);
  cache.MaybeEnqueueReport(network_context_.get(), host_port_pair, chain_.get(),
                           sct_list);

  ASSERT_EQ(0u, cache.GetCacheForTesting()->size());
}

// Test that inserting and retrieving a report works.
TEST_F(SCTAuditingCacheTest, InsertAndRetrieveReport) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters({probability_one}, {});
  SCTAuditingCache cache(10);

  const net::HostPortPair host_port_pair("example.com", 443);
  net::SignedCertificateTimestampAndStatusList sct_list;
  MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                       "extensions1", "signature1", base::Time::Now(),
                       net::ct::SCT_STATUS_LOG_UNKNOWN, &sct_list);
  cache.MaybeEnqueueReport(network_context_.get(), host_port_pair, chain_.get(),
                           sct_list);

  ASSERT_EQ(1u, cache.GetCacheForTesting()->size());
}

// Tests that old entries are evicted when the cache is full.
TEST_F(SCTAuditingCacheTest, EvictLRUAfterCacheFull) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters({probability_one}, {});
  SCTAuditingCache cache(2);

  const net::HostPortPair host_port_pair1("example1.com", 443);
  const net::HostPortPair host_port_pair2("example2.com", 443);
  const net::HostPortPair host_port_pair3("example3.com", 443);

  {
    net::SignedCertificateTimestampAndStatusList sct_list;
    MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                         "extensions1", "signature1", base::Time::Now(),
                         net::ct::SCT_STATUS_LOG_UNKNOWN, &sct_list);
    cache.MaybeEnqueueReport(network_context_.get(), host_port_pair1,
                             chain_.get(), sct_list);
    ASSERT_EQ(1u, cache.GetCacheForTesting()->size());
  }

  {
    net::SignedCertificateTimestampAndStatusList sct_list;
    MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                         "extensions1", "signature2", base::Time::Now(),
                         net::ct::SCT_STATUS_LOG_UNKNOWN, &sct_list);
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
                         net::ct::SCT_STATUS_LOG_UNKNOWN, &sct_list);
    cache.MaybeEnqueueReport(network_context_.get(), host_port_pair3,
                             chain_.get(), sct_list);
    ASSERT_EQ(2u, cache.GetCacheForTesting()->size());
    for (const auto& entry : *cache.GetCacheForTesting()) {
      ASSERT_NE("example1.com", entry.second->context().origin().hostname());
    }
  }
}

// Tests that a new report gets dropped if the same SCTs are already in the
// cache.
TEST_F(SCTAuditingCacheTest, ReportWithSameSCTsDeduplicated) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters({probability_one}, {});
  SCTAuditingCache cache(10);

  const net::HostPortPair host_port_pair1("example.com", 443);
  const net::HostPortPair host_port_pair2("example.org", 443);

  net::SignedCertificateTimestampAndStatusList sct_list;
  MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                       "extensions1", "signature1", base::Time::Now(),
                       net::ct::SCT_STATUS_LOG_UNKNOWN, &sct_list);
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
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters({probability_one}, {});
  SCTAuditingCache cache(2);

  const net::HostPortPair host_port_pair1("example1.com", 443);
  const net::HostPortPair host_port_pair2("example2.com", 443);
  const net::HostPortPair host_port_pair3("example3.com", 443);

  // Fill the cache with two reports.
  net::SignedCertificateTimestampAndStatusList sct_list1;
  MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                       "extensions1", "signature1", base::Time::Now(),
                       net::ct::SCT_STATUS_LOG_UNKNOWN, &sct_list1);
  cache.MaybeEnqueueReport(network_context_.get(), host_port_pair1,
                           chain_.get(), sct_list1);

  net::SignedCertificateTimestampAndStatusList sct_list2;
  MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                       "extensions2", "signature2", base::Time::Now(),
                       net::ct::SCT_STATUS_LOG_UNKNOWN, &sct_list2);
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
                       net::ct::SCT_STATUS_LOG_UNKNOWN, &sct_list3);
  cache.MaybeEnqueueReport(network_context_.get(), host_port_pair3,
                           chain_.get(), sct_list3);

  EXPECT_EQ(2u, cache.GetCacheForTesting()->size());
  for (const auto& entry : *cache.GetCacheForTesting()) {
    ASSERT_NE("example2.com", entry.second->context().origin().hostname());
  }
}

}  // namespace network
