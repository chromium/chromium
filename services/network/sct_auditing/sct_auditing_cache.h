// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SCT_AUDITING_SCT_AUDITING_CACHE_H_
#define SERVICES_NETWORK_SCT_AUDITING_SCT_AUDITING_CACHE_H_

#include <optional>

#include "base/component_export.h"
#include "base/containers/lru_cache.h"
#include "base/timer/timer.h"
#include "net/base/hash_value.h"
#include "net/base/host_port_pair.h"
#include "net/cert/sct_auditing_delegate.h"
#include "net/cert/signed_certificate_timestamp_and_status.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/proto/sct_audit_report.pb.h"
#include "url/gurl.h"

namespace net {
class X509Certificate;
}

namespace network {

class NetworkContext;

// SCTAuditingCache is the main entrypoint for new SCT auditing reports. A
// single SCTAuditingCache should be shared among all contexts that want to
// deduplicate reports and use a single sampling mechanism. Currently, one
// SCTAuditingCache is created and owned by the NetworkService and shared
// across all NetworkContexts.
//
// SCTAuditingCache tracks SCTs seen during CT verification. The cache supports
// a configurable sample rate to reduce load, and deduplicates SCTs seen more
// than once. The cache evicts least-recently-used entries after it reaches its
// capacity.
//
// Once the SCTAuditingCache has selected a report to be sampled, it creates a
// new SCTAuditingReporter and passes it to the SCTAuditingHandler for the
// NetworkContext that triggered the report. The actual reporting and retrying
// logic is handled by one SCTAuditingReporter per report. Pending reporters are
// owned by the SCTAuditingHandler.
//
// The SCTAuditingCache allows the embedder to configure SCT auditing via the
// network service's ConfigureSCTAuditing() API.
//
// Note: The SCTAuditingCache's deduplication cache is not persisted to disk.
// Pending reports that are persisted to disk by SCTAuditingHandler do not
// repopulate the deduplication cache when loaded. Not persisting the dedupe
// cache slightly increases the probability weight of sampling and sending SCTs
// from sites a user commonly visits (i.e., those they are likely to visit in
// every session).
class COMPONENT_EXPORT(NETWORK_SERVICE) SCTAuditingCache {
 public:
  struct COMPONENT_EXPORT(NETWORK_SERVICE) ReportEntry {
    ReportEntry();
    ~ReportEntry();
    ReportEntry(const ReportEntry&) = delete;
    ReportEntry operator==(const ReportEntry&) = delete;
    ReportEntry(ReportEntry&&);
    net::HashValue key;
    std::unique_ptr<sct_auditing::SCTClientReport> report;
  };

  explicit SCTAuditingCache(size_t cache_size = 1024);
  ~SCTAuditingCache();

  SCTAuditingCache(const SCTAuditingCache&) = delete;
  SCTAuditingCache& operator=(const SCTAuditingCache&) = delete;

  // Configures the cache. Must be called before |MaybeGenerateReportEntry| and
  // |GetConfiguration|.
  void Configure(mojom::SCTAuditingConfigurationPtr configuration);

  // Returns a copy of the SCT configuration.
  mojom::SCTAuditingConfigurationPtr GetConfiguration() const;

  // Creates a report containing the details about the connection context and
  // SCTs and adds it to the cache if the SCTs are not already in the
  // cache. If the SCTs were not already in the cache, a random sample is drawn
  // to determine whether to send a report. This means we sample a subset of
  // *certificates* rather than a subset of *connections*.
  // Returns the report entry if the report should be sent, and std::nullopt
  // otherwise.
  std::optional<ReportEntry> MaybeGenerateReportEntry(
      const net::HostPortPair& host_port_pair,
      const net::X509Certificate* validated_certificate_chain,
      const net::SignedCertificateTimestampAndStatusList&
          signed_certificate_timestamps);

  // Returns true if |sct_leaf_hash| corresponds to a known popular SCT.
  bool IsPopularSCT(base::span<const uint8_t> sct_leaf_hash);

  void ClearCache();

  void set_popular_scts(std::vector<std::vector<uint8_t>> popular_scts) {
    popular_scts_ = popular_scts;
  }

  base::LRUCache<net::HashValue, bool>* GetCacheForTesting() {
    return &dedupe_cache_;
  }

 private:
  void ReportHWMMetrics();

  // Value `bool` is ignored in the dedupe cache. This cache only stores
  // recently seen hashes of SCTs in order to deduplicate on SCTs, and the bool
  // will always be `true`.
  base::LRUCache<net::HashValue, bool> dedupe_cache_;
  // Tracks high-water-mark of `dedupe_cache_.size()`.
  size_t dedupe_cache_size_hwm_ = 0;

  // A list of hashes for popular SCTs that should not be scheduled for auditing
  // as an optimization for hashdance clients.
  std::vector<std::vector<uint8_t>> popular_scts_;

  mojom::SCTAuditingConfigurationPtr configuration_;

  base::RepeatingTimer histogram_timer_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_SCT_AUDITING_SCT_AUDITING_CACHE_H_
