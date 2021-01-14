// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SCT_AUDITING_CACHE_H_
#define SERVICES_NETWORK_SCT_AUDITING_CACHE_H_

#include <map>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/mru_cache.h"
#include "base/time/time.h"
#include "net/base/hash_value.h"
#include "net/base/host_port_pair.h"
#include "net/cert/sct_auditing_delegate.h"
#include "net/cert/signed_certificate_timestamp_and_status.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_request.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/proto/sct_audit_report.pb.h"

namespace net {
class X509Certificate;
}

namespace network {
class NetworkContext;

// SCTAuditingCache tracks SCTs seen during CT verification. The cache supports
// a configurable sample rate to reduce load, and deduplicates SCTs seen more
// than once. The cache evicts least-recently-used entries after it reaches its
// capacity.
//
// The SCTAuditingCache also handles sending reports to a specified report URL
// using a specific NetworkContext. These are configured by the embedder via the
// network service's ConfigureSCTAuditing() API.
//
// A single SCTAuditingCache should be shared among all contexts that want to
// deduplicate reports and use a single sampling mechanism. Currently, one
// SCTAuditingCache is created and owned by the NetworkService and shared
// across all NetworkContexts.
class COMPONENT_EXPORT(NETWORK_SERVICE) SCTAuditingCache {
 public:
  explicit SCTAuditingCache(size_t cache_size);
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

  sct_auditing::SCTClientReport* GetPendingReport(
      const net::SHA256HashValue& cache_key);

  // Sends the report associated with `cache_key` to `report_uri` (which is
  // specified by the embedder). When the request completes (on success or
  // failure), `callback` will be called with the response details.
  void SendReport(const net::SHA256HashValue& cache_key);

  void ClearCache();

  void set_enabled(bool enabled) { enabled_ = enabled; }
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

  base::MRUCache<net::SHA256HashValue,
                 std::unique_ptr<sct_auditing::SCTClientReport>>*
  GetCacheForTesting() {
    return &cache_;
  }

 private:
  void OnReportComplete(const net::SHA256HashValue& cache_key,
                        int net_error,
                        int http_response_code);

  base::MRUCache<net::SHA256HashValue,
                 std::unique_ptr<sct_auditing::SCTClientReport>>
      cache_;

  bool enabled_ = false;
  double sampling_rate_ = 0;
  GURL report_uri_;
  net::MutableNetworkTrafficAnnotationTag traffic_annotation_;
  mojo::Remote<mojom::URLLoaderFactory> url_loader_factory_;

  size_t cache_size_hwm_;

  base::WeakPtrFactory<SCTAuditingCache> weak_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_SCT_AUDITING_CACHE_H_
