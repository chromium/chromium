// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/sct_auditing/sct_auditing_cache.h"

#include "base/callback.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "components/version_info/version_info.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"
#include "net/base/elements_upload_data_stream.h"
#include "net/base/hash_value.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/cert/ct_serialization.h"
#include "net/cert/sct_status_flags.h"
#include "net/cert/signed_certificate_timestamp.h"
#include "net/cert/signed_certificate_timestamp_and_status.h"
#include "net/cert/x509_certificate.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/public/proto/sct_audit_report.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/boringssl/src/include/openssl/pool.h"
#include "third_party/boringssl/src/include/openssl/sha.h"

namespace network {

namespace {

constexpr int kSendSCTReportTimeoutSeconds = 30;

// Overrides the initial retry delay in SCTAuditingReporter::kBackoffPolicy if
// not nullopt.
absl::optional<base::TimeDelta> g_retry_delay_for_testing = absl::nullopt;

// Records the high-water mark of the cache size (in number of reports).
void RecordSCTAuditingCacheHighWaterMarkMetrics(size_t cache_hwm,
                                                size_t reporters_hwm) {
  base::UmaHistogramCounts1000("Security.SCTAuditing.OptIn.DedupeCacheHWM",
                               cache_hwm);
  base::UmaHistogramCounts1000("Security.SCTAuditing.OptIn.ReportersHWM",
                               reporters_hwm);
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

// Records whether sending the report to the reporting server succeeded for each
// report sent.
void RecordSCTAuditingReportSucceededMetrics(bool success) {
  base::UmaHistogramBoolean("Security.SCTAuditing.OptIn.ReportSucceeded",
                            success);
}

// Records whether a report succeeded/failed with retries.
void ReportSCTAuditingCompletionStatusMetrics(
    SCTAuditingReporter::CompletionStatus status) {
  base::UmaHistogramEnumeration(
      "Security.SCTAuditing.OptIn.ReportCompletionStatus", status);
}

}  // namespace

// static
const net::BackoffEntry::Policy SCTAuditingReporter::kDefaultBackoffPolicy = {
    // Don't ignore initial errors; begin exponential back-off rules immediately
    // if the first attempt fails.
    .num_errors_to_ignore = 0,
    // Start with a 30s delay, including for the first attempt (due to setting
    // `always_use_initial_delay = true` below).
    .initial_delay_ms = 30 * 1000,
    // Double the backoff delay each retry.
    .multiply_factor = 2.0,
    // Spread requests randomly between 80-100% of the calculated backoff time.
    .jitter_factor = 0.2,
    // Max retry delay is 1 day.
    .maximum_backoff_ms = 24 * 60 * 60 * 1000,
    // Never discard the entry.
    .entry_lifetime_ms = -1,
    // Initial attempt will be delayed (and jittered). This reduces the risk of
    // a "thundering herd" of reports on startup if a user has many reports
    // persisted (once pending reports are persisted and recreated at startup
    // with a new BackoffEntry).
    .always_use_initial_delay = true,
};

// How many times an SCTAuditingReporter should retry sending an audit report.
// Given kDefaultBackoffPolicy above and 15 total retries, this means there will
// be five attempts in the first 30 minutes and then occasional retries for
// roughly the next five days.
// See more discussion in the SCT Auditing Retry and Persistence design doc:
// https://docs.google.com/document/d/1YTUzoG6BDF1QIxosaQDp2H5IzYY7_fwH8qNJXSVX8OQ/edit
constexpr size_t kMaxRetries = 15;

SCTAuditingReporter::SCTAuditingReporter(
    net::SHA256HashValue reporter_key,
    std::unique_ptr<sct_auditing::SCTClientReport> report,
    mojom::URLLoaderFactory& url_loader_factory,
    const GURL& report_uri,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    ReporterDoneCallback done_callback)
    : reporter_key_(reporter_key),
      report_(std::move(report)),
      traffic_annotation_(traffic_annotation),
      report_uri_(report_uri),
      done_callback_(std::move(done_callback)),
      num_retries_(0),
      max_retries_(kMaxRetries) {
  // Clone the URLLoaderFactory to avoid any dependencies on its lifetime. The
  // Reporter instance can maintain its own copy.
  // Relatively few Reporters are expected to exist at a time (due to sampling
  // and deduplication), so some cost of copying is reasonable. If more
  // optimization is needed, this could potentially use a mojo::SharedRemote
  // or a WrappedPendingSharedURLLoaderFactory instead.
  url_loader_factory.Clone(
      url_loader_factory_remote_.BindNewPipeAndPassReceiver());

  // Override the retry delay if set by tests.
  backoff_policy_ = kDefaultBackoffPolicy;
  if (g_retry_delay_for_testing) {
    backoff_policy_.initial_delay_ms =
        g_retry_delay_for_testing->InMilliseconds();
  }
  backoff_entry_ = std::make_unique<net::BackoffEntry>(&backoff_policy_);

  // Informing the backoff entry of a success will force it to use the initial
  // delay (and jitter) for the first attempt. Otherwise, ShouldRejectRequest()
  // will return `true` despite the policy specifying
  // `always_use_initial_delay = true`.
  backoff_entry_->InformOfRequest(true);

  // Start sending the report.
  ScheduleReport();
}

SCTAuditingReporter::~SCTAuditingReporter() = default;

void SCTAuditingReporter::ScheduleReport() {
  if (base::FeatureList::IsEnabled(
          features::kSCTAuditingRetryAndPersistReports) &&
      backoff_entry_->ShouldRejectRequest()) {
    // TODO(crbug.com/1199827): Investigate if explicit task traits should be
    // used for these tasks (e.g., BEST_EFFORT and SKIP_ON_SHUTDOWN).
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&SCTAuditingReporter::SendReport,
                       weak_factory_.GetWeakPtr()),
        backoff_entry_->GetTimeUntilRelease());
  } else {
    SendReport();
  }
}

void SCTAuditingReporter::SendReport() {
  DCHECK(url_loader_factory_remote_);

  // Create a SimpleURLLoader for the request.
  auto report_request = std::make_unique<ResourceRequest>();
  report_request->url = report_uri_;
  report_request->method = "POST";
  report_request->load_flags = net::LOAD_DISABLE_CACHE;
  report_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  url_loader_ = SimpleURLLoader::Create(
      std::move(report_request),
      static_cast<net::NetworkTrafficAnnotationTag>(traffic_annotation_));
  url_loader_->SetTimeoutDuration(
      base::TimeDelta::FromSeconds(kSendSCTReportTimeoutSeconds));
  // Retry is handled by SCTAuditingReporter.
  url_loader_->SetRetryOptions(0, SimpleURLLoader::RETRY_NEVER);

  // Serialize the report and attach it to the loader.
  // TODO(crbug.com/1199566): Should we store the serialized report instead, so
  // we don't serialize on every retry?
  std::string report_data;
  bool ok = report_->SerializeToString(&report_data);
  DCHECK(ok);
  url_loader_->AttachStringForUpload(report_data, "application/octet-stream");

  // The server acknowledges receiving the report via a successful HTTP status
  // with no response body, so this uses DownloadHeadersOnly for simplicity.
  // If the loader is destroyed, the callback will be canceled, so using
  // base::Unretained here is safe.
  url_loader_->DownloadHeadersOnly(
      url_loader_factory_remote_.get(),
      base::BindOnce(&SCTAuditingReporter::OnSendReportComplete,
                     base::Unretained(this)));
}

void SCTAuditingReporter::OnSendReportComplete(
    scoped_refptr<net::HttpResponseHeaders> headers) {
  int response_code = 0;
  if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers) {
    response_code = url_loader_->ResponseInfo()->headers->response_code();
  }
  bool success =
      url_loader_->NetError() == net::OK && response_code == net::HTTP_OK;

  RecordSCTAuditingReportSucceededMetrics(success);

  if (base::FeatureList::IsEnabled(
          features::kSCTAuditingRetryAndPersistReports)) {
    if (success) {
      // Report succeeded.
      if (num_retries_ == 0) {
        ReportSCTAuditingCompletionStatusMetrics(
            CompletionStatus::kSuccessFirstTry);
      } else {
        ReportSCTAuditingCompletionStatusMetrics(
            CompletionStatus::kSuccessAfterRetries);
      }

      // Notify the Cache that this Reporter is done. This will delete |this|,
      // so do not add code after this point.
      std::move(done_callback_).Run(reporter_key_);
      return;
    }
    // Sending the report failed.
    if (num_retries_ >= max_retries_) {
      // Retry limit reached.
      ReportSCTAuditingCompletionStatusMetrics(
          CompletionStatus::kRetriesExhausted);

      // Notify the Cache that this Reporter is done. This will delete |this|,
      // so do not add code after this point.
      std::move(done_callback_).Run(reporter_key_);
      return;
    } else {
      // Schedule a retry.
      ++num_retries_;
      backoff_entry_->InformOfRequest(false);
      ScheduleReport();
    }
  } else {
    // Retry is not enabled, so just notify the Cache that this Reporter is
    // done. This will delete |this|, so do not add code after this point.
    std::move(done_callback_).Run(reporter_key_);
    return;
  }
}

SCTAuditingCache::SCTAuditingCache(size_t cache_size)
    : dedupe_cache_(cache_size),
      dedupe_cache_size_hwm_(0),
      pending_reporters_(cache_size),
      pending_reporters_size_hwm_(0) {}

SCTAuditingCache::~SCTAuditingCache() = default;

void SCTAuditingCache::MaybeEnqueueReport(
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

  // Create a Reporter, which will own the report and handle sending and
  // retrying. When complete, the Reporter will call the callback which will
  // handle deleting the Reporter. The callback takes a WeakPtr in case the
  // SCTAuditingCache or Network Service were destroyed before the callback
  // triggers.
  pending_reporters_.Put(
      cache_key, std::make_unique<SCTAuditingReporter>(
                     cache_key, std::move(report), *url_loader_factory_,
                     report_uri_, traffic_annotation_,
                     base::BindOnce(&SCTAuditingCache::OnReporterFinished,
                                    weak_factory_.GetWeakPtr())));
  if (pending_reporters_.size() > pending_reporters_size_hwm_)
    pending_reporters_size_hwm_ = pending_reporters_.size();
}

void SCTAuditingCache::OnReporterFinished(net::SHA256HashValue reporter_key) {
  auto it = pending_reporters_.Get(reporter_key);
  if (it != pending_reporters_.end()) {
    pending_reporters_.Erase(it);
  }

  if (completion_callback_for_testing_)
    std::move(completion_callback_for_testing_).Run();

  // TODO(crbug.com/1144205): Delete any persisted state for the reporter.
}

void SCTAuditingCache::ClearCache() {
  // Empty the deduplication cache.
  dedupe_cache_.Clear();
  // Delete any outstanding Reporters. This will delete any extant URLLoader
  // instances owned by the Reporters, which will cancel any outstanding
  // requests/connections. Pending (delayed) retry tasks will fast-fail when
  // they trigger as they use a WeakPtr to the Reporter instance that posted the
  // task.
  pending_reporters_.Clear();
  // TODO(crbug.com/1144205): Clear any persisted state.
}

void SCTAuditingCache::set_enabled(bool enabled) {
  enabled_ = enabled;
  SetPeriodicMetricsEnabled(enabled);
}

void SCTAuditingCache::SetRetryDelayForTesting(
    absl::optional<base::TimeDelta> delay) {
  g_retry_delay_for_testing = delay;
}

void SCTAuditingCache::SetCompletionCallbackForTesting(
    base::OnceClosure callback) {
  completion_callback_for_testing_ = std::move(callback);
}

void SCTAuditingCache::ReportHWMMetrics() {
  if (!enabled_)
    return;
  RecordSCTAuditingCacheHighWaterMarkMetrics(dedupe_cache_size_hwm_,
                                             pending_reporters_size_hwm_);
}

void SCTAuditingCache::SetPeriodicMetricsEnabled(bool enabled) {
  // High-water-mark metrics get logged hourly (rather than once-per-session at
  // shutdown, as Network Service shutdown is not consistent and non-browser
  // processes can fail to report metrics during shutdown). The timer should
  // only be running if SCT auditing is enabled.
  if (enabled) {
    histogram_timer_.Start(FROM_HERE, base::TimeDelta::FromHours(1), this,
                           &SCTAuditingCache::ReportHWMMetrics);
  } else {
    histogram_timer_.Stop();
  }
}

}  // namespace network
