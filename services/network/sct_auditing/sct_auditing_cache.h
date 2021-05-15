// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SCT_AUDITING_SCT_AUDITING_CACHE_H_
#define SERVICES_NETWORK_SCT_AUDITING_SCT_AUDITING_CACHE_H_

#include <map>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/mru_cache.h"
#include "base/time/time.h"
#include "net/base/backoff_entry.h"
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

class SimpleURLLoader;

// Owns an SCT auditing report and handles sending it and retrying on failures.
// An SCTAuditingReporter begins trying to send the report to the report server
// on creation, retrying over time based on the backoff policy defined in
// `kBackoffPolicy`, and then runs the ReporterDoneCallback provided by the
// SCTAuditingCache to notify the cache when it has completed (either success or
// running out of retries). If an SCTAuditingReporter instance is deleted, any
// outstanding requests are canceled, pending retry tasks will fail, and
// `done_callback` will not called.
//
// An SCTAuditingReporter instance is uniquely identified by its `reporter_key`
// hash, which is the SHA256HashValue of the set of SCTs included in the
// SCTClientReport that the reporter owns.
//
// Declared here for testing visibility.
class SCTAuditingReporter {
 public:
  // Callback to notify the SCTAuditingCache that this reporter has completed.
  // The SHA256HashValue `reporter_key` is passed to uniquely identify this
  // reporter instance.
  using ReporterDoneCallback = base::OnceCallback<void(net::SHA256HashValue)>;

  SCTAuditingReporter(
      net::SHA256HashValue reporter_key,
      std::unique_ptr<sct_auditing::SCTClientReport> report,
      mojom::URLLoaderFactory& url_loader_factory,
      const GURL& report_uri,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      ReporterDoneCallback done_callback);
  ~SCTAuditingReporter();

  SCTAuditingReporter(const SCTAuditingReporter&) = delete;
  SCTAuditingReporter& operator=(const SCTAuditingReporter&) = delete;

  static const net::BackoffEntry::Policy kDefaultBackoffPolicy;

  sct_auditing::SCTClientReport* report() { return report_.get(); }

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class CompletionStatus {
    kSuccessFirstTry = 0,
    kSuccessAfterRetries = 1,
    kRetriesExhausted = 2,
    kMaxValue = kRetriesExhausted,
  };

 private:
  void ScheduleReport();
  void SendReport();
  void OnSendReportComplete(scoped_refptr<net::HttpResponseHeaders> headers);

  net::SHA256HashValue reporter_key_;
  std::unique_ptr<sct_auditing::SCTClientReport> report_;
  mojo::Remote<mojom::URLLoaderFactory> url_loader_factory_remote_;
  std::unique_ptr<SimpleURLLoader> url_loader_;
  net::NetworkTrafficAnnotationTag traffic_annotation_;
  GURL report_uri_;
  ReporterDoneCallback done_callback_;

  net::BackoffEntry::Policy backoff_policy_;
  std::unique_ptr<net::BackoffEntry> backoff_entry_;

  size_t num_retries_;
  size_t max_retries_;

  base::WeakPtrFactory<SCTAuditingReporter> weak_factory_{this};
};

// SCTAuditingCache tracks SCTs seen during CT verification. The cache supports
// a configurable sample rate to reduce load, and deduplicates SCTs seen more
// than once. The cache evicts least-recently-used entries after it reaches its
// capacity.
//
// The SCTAuditingCache also handles sending reports to a specified report URI
// using a specified URLLoaderFactory (for a specific NetworkContext). These
// are configured by the embedder via the network service's
// ConfigureSCTAuditing() API. The actual reporting and retrying logic is
// handled by one SCTAuditingReporter per report. Pending reporters are owned
// by the SCTAuditingCache and tracked in the `pending_reporters_` set.
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

  base::MRUCache<net::SHA256HashValue, bool>* GetCacheForTesting() {
    return &dedupe_cache_;
  }

  base::MRUCache<net::SHA256HashValue, std::unique_ptr<SCTAuditingReporter>>*
  GetPendingReportersForTesting() {
    return &pending_reporters_;
  }

  void SetRetryDelayForTesting(absl::optional<base::TimeDelta> delay);

  void SetCompletionCallbackForTesting(base::OnceClosure callback);

 private:
  void OnReporterFinished(net::SHA256HashValue reporter_key);
  void ReportHWMMetrics();
  void SetPeriodicMetricsEnabled(bool enabled);

  // Value `bool` is ignored in the dedupe cache. This cache only stores
  // recently seen hashes of SCTs in order to deduplicate on SCTs, and the bool
  // will always be `true`.
  base::MRUCache<net::SHA256HashValue, bool> dedupe_cache_;
  // Tracks high-water-mark of `dedupe_cache_.size()`.
  size_t dedupe_cache_size_hwm_;

  // The pending reporters set is an MRUCache, so that the total number of
  // pending reporters can be capped. The MRUCache means that reporters will be
  // evicted (and canceled) oldest first. If a new report is triggered for the
  // same SCTs it will get deduplicated if a previous report is still pending,
  // but the last-seen time will be updated.
  base::MRUCache<net::SHA256HashValue, std::unique_ptr<SCTAuditingReporter>>
      pending_reporters_;
  // Tracks high-water-mark of `pending_reporters_.size()`.
  size_t pending_reporters_size_hwm_;

  bool enabled_ = false;
  double sampling_rate_ = 0;
  GURL report_uri_;
  net::MutableNetworkTrafficAnnotationTag traffic_annotation_;
  mojo::Remote<mojom::URLLoaderFactory> url_loader_factory_;

  base::OnceClosure completion_callback_for_testing_;

  base::RepeatingTimer histogram_timer_;

  base::WeakPtrFactory<SCTAuditingCache> weak_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_SCT_AUDITING_SCT_AUDITING_CACHE_H_
