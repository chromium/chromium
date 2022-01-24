// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SCT_AUDITING_SCT_AUDITING_CACHE_H_
#define SERVICES_NETWORK_SCT_AUDITING_SCT_AUDITING_CACHE_H_

#include <map>
#include <vector>

#include "base/component_export.h"
#include "base/containers/lru_cache.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/hash_value.h"
#include "net/base/host_port_pair.h"
#include "net/cert/sct_auditing_delegate.h"
#include "net/cert/signed_certificate_timestamp_and_status.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/proto/sct_audit_report.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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
class COMPONENT_EXPORT(NETWORK_SERVICE) SCTAuditingCache {
 public:
  explicit SCTAuditingCache(size_t cache_size = 1024);
  ~SCTAuditingCache();

  SCTAuditingCache(const SCTAuditingCache&) = delete;
  SCTAuditingCache& operator=(const SCTAuditingCache&) = delete;

  // Creates a report containing the details about the connection context and
  // SCTs and adds it to the cache if the SCTs are not already in the
  // cache. If the SCTs were not already in the cache, a random sample is drawn
  // to determine whether to send a report. This means we sample a subset of
  // *certificates* rather than a subset of *connections*.
  void MaybeEnqueueReport(
      NetworkContext* context,
      const net::HostPortPair& host_port_pair,
      const net::X509Certificate* validated_certificate_chain,
      const net::SignedCertificateTimestampAndStatusList&
          signed_certificate_timestamps);

  void ClearCache();

  void set_enabled(bool enabled);
  void set_sampling_rate(double rate) { sampling_rate_ = rate; }
  void set_report_uri(const GURL& report_uri) { report_uri_ = report_uri; }
  void set_traffic_annotation(
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
    traffic_annotation_ = traffic_annotation;
  }
  void set_url_loader_factory(
      mojo::PendingRemote<mojom::URLLoaderFactory> factory) {
    url_loader_factory_.Bind(std::move(factory));
  }

  base::LRUCache<net::SHA256HashValue, bool>* GetCacheForTesting() {
    return &dedupe_cache_;
  }

 private:
  void ReportHWMMetrics();
  void SetPeriodicMetricsEnabled(bool enabled);

  // Value `bool` is ignored in the dedupe cache. This cache only stores
  // recently seen hashes of SCTs in order to deduplicate on SCTs, and the bool
  // will always be `true`.
  base::LRUCache<net::SHA256HashValue, bool> dedupe_cache_;
  // Tracks high-water-mark of `dedupe_cache_.size()`.
  size_t dedupe_cache_size_hwm_ = 0;

  bool enabled_ = false;
  double sampling_rate_ = 0;
  GURL report_uri_;
  net::MutableNetworkTrafficAnnotationTag traffic_annotation_;
  mojo::Remote<mojom::URLLoaderFactory> url_loader_factory_;

  base::RepeatingTimer histogram_timer_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_SCT_AUDITING_SCT_AUDITING_CACHE_H_
