// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/sct_auditing/sct_auditing_cache.h"

#include "base/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "components/version_info/version_info.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"
#include "net/base/hash_value.h"
#include "net/cert/ct_serialization.h"
#include "net/cert/sct_status_flags.h"
#include "net/cert/signed_certificate_timestamp.h"
#include "net/cert/signed_certificate_timestamp_and_status.h"
#include "net/cert/x509_certificate.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "services/network/network_context.h"
#include "services/network/public/proto/sct_audit_report.pb.h"
#include "services/network/sct_auditing/sct_auditing_handler.h"
#include "third_party/boringssl/src/include/openssl/pool.h"
#include "third_party/boringssl/src/include/openssl/sha.h"

namespace network {

namespace {

// Records the high-water mark of the cache size (in number of reports).
void RecordSCTAuditingCacheHighWaterMarkMetrics(size_t cache_hwm) {
  base::UmaHistogramCounts1000("Security.SCTAuditing.OptIn.DedupeCacheHWM",
                               cache_hwm);
}

// Records whether a new report is deduplicated against an existing report in
// the cache.
void RecordSCTAuditingReportDeduplicatedMetrics(bool deduplicated) {
  base::UmaHistogramBoolean("Security.SCTAuditing.OptIn.ReportDeduplicated",
                            deduplicated);
}

// Records whether a new report that wasn't deduplicated was sampled for
// sending to the reporting server.
void RecordSCTAuditingReportSampledMetrics(bool sampled) {
  base::UmaHistogramBoolean("Security.SCTAuditing.OptIn.ReportSampled",
                            sampled);
}

// Records the size of a report that will be sent to the reporting server, in
// bytes. Used to track how much bandwidth is consumed by sending reports.
void RecordSCTAuditingReportSizeMetrics(size_t report_size) {
  base::UmaHistogramCounts10M("Security.SCTAuditing.OptIn.ReportSize",
                              report_size);
}

}  // namespace

SCTAuditingCache::SCTAuditingCache(size_t cache_size)
    : dedupe_cache_(cache_size) {}

SCTAuditingCache::~SCTAuditingCache() = default;

void SCTAuditingCache::MaybeEnqueueReport(
    NetworkContext* context,
    const net::HostPortPair& host_port_pair,
    const net::X509Certificate* validated_certificate_chain,
    const net::SignedCertificateTimestampAndStatusList&
        signed_certificate_timestamps) {
  if (!enabled_)
    return;

  auto report = std::make_unique<sct_auditing::SCTClientReport>();
  auto* tls_report = report->add_certificate_report();

  // Encode the SCTs in the report and generate the cache key. The hash of the
  // SCTs is used as the cache key to deduplicate reports with the same SCTs.
  // Constructing the report in parallel with computing the hash avoids
  // encoding the SCTs multiple times and avoids extra copies.
  SHA256_CTX ctx;
  SHA256_Init(&ctx);
  for (const auto& sct : signed_certificate_timestamps) {
    // Only audit valid SCTs. This ensures that they come from a known log, have
    // a valid signature, and thus are expected to be public certificates. If
    // there are no valid SCTs, there's no need to report anything.
    if (sct.status != net::ct::SCT_STATUS_OK)
      continue;

    auto* sct_source_and_status = tls_report->add_included_sct();
    // TODO(crbug.com/1082860): Update the proto to remove the status entirely
    // since only valid SCTs are reported now.
    sct_source_and_status->set_status(
        sct_auditing::SCTWithVerifyStatus::SctVerifyStatus::
            SCTWithVerifyStatus_SctVerifyStatus_OK);

    net::ct::EncodeSignedCertificateTimestamp(
        sct.sct, sct_source_and_status->mutable_serialized_sct());

    SHA256_Update(&ctx, sct_source_and_status->serialized_sct().data(),
                  sct_source_and_status->serialized_sct().size());
  }
  // Don't handle reports if there were no valid SCTs.
  if (tls_report->included_sct().empty())
    return;

  net::SHA256HashValue cache_key;
  SHA256_Final(reinterpret_cast<uint8_t*>(&cache_key), &ctx);

  // Check if the SCTs are already in the cache. This will update the last seen
  // time if they are present in the cache.
  auto it = dedupe_cache_.Get(cache_key);
  if (it != dedupe_cache_.end()) {
    RecordSCTAuditingReportDeduplicatedMetrics(true);
    return;
  }
  RecordSCTAuditingReportDeduplicatedMetrics(false);

  report->set_user_agent(version_info::GetProductNameAndVersionForUserAgent());

  // Add `cache_key` to the dedupe cache. The cache value is not used.
  dedupe_cache_.Put(cache_key, true);

  if (base::RandDouble() > sampling_rate_) {
    RecordSCTAuditingReportSampledMetrics(false);
    return;
  }
  RecordSCTAuditingReportSampledMetrics(true);

  auto* connection_context = tls_report->mutable_context();
  base::TimeDelta time_since_unix_epoch =
      base::Time::Now() - base::Time::UnixEpoch();
  connection_context->set_time_seen(time_since_unix_epoch.InSeconds());
  auto* origin = connection_context->mutable_origin();
  origin->set_hostname(host_port_pair.host());
  origin->set_port(host_port_pair.port());

  // Convert the certificate chain to a PEM encoded vector, and then initialize
  // the proto's |certificate_chain| repeated field using the data in the
  // vector. Note that GetPEMEncodedChain() can fail, but we still want to
  // enqueue the report for the SCTs (in that case, |certificate_chain| is not
  // guaranteed to be valid).
  std::vector<std::string> certificate_chain;
  validated_certificate_chain->GetPEMEncodedChain(&certificate_chain);
  *connection_context->mutable_certificate_chain() = {certificate_chain.begin(),
                                                      certificate_chain.end()};

  // Log the size of the report. This only tracks reports that are not dropped
  // due to sampling (as those reports will just be empty).
  RecordSCTAuditingReportSizeMetrics(report->ByteSizeLong());

  // Track high-water-mark for the size of the cache.
  if (dedupe_cache_.size() > dedupe_cache_size_hwm_)
    dedupe_cache_size_hwm_ = dedupe_cache_.size();

  // Ensure that the URLLoaderFactory is still bound.
  if (!url_loader_factory_ || !url_loader_factory_.is_connected()) {
    // TODO(cthomp): Should this signal to embedder that something has failed?
    return;
  }

  context->sct_auditing_handler()->AddReporter(
      cache_key, std::move(report), *url_loader_factory_, report_uri_,
      traffic_annotation_);
}

void SCTAuditingCache::ClearCache() {
  // Empty the deduplication cache.
  dedupe_cache_.Clear();
}

void SCTAuditingCache::set_enabled(bool enabled) {
  enabled_ = enabled;
  SetPeriodicMetricsEnabled(enabled);
}

void SCTAuditingCache::ReportHWMMetrics() {
  if (!enabled_)
    return;
  RecordSCTAuditingCacheHighWaterMarkMetrics(dedupe_cache_size_hwm_);
}

void SCTAuditingCache::SetPeriodicMetricsEnabled(bool enabled) {
  // High-water-mark metrics get logged hourly (rather than once-per-session at
  // shutdown, as Network Service shutdown is not consistent and non-browser
  // processes can fail to report metrics during shutdown). The timer should
  // only be running if SCT auditing is enabled.
  if (enabled) {
    histogram_timer_.Start(FROM_HERE, base::Hours(1), this,
                           &SCTAuditingCache::ReportHWMMetrics);
  } else {
    histogram_timer_.Stop();
  }
}

}  // namespace network
