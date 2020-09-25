// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/sct_auditing_cache.h"

#include "base/callback.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
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
#include "net/cert/signed_certificate_timestamp.h"
#include "net/cert/signed_certificate_timestamp_and_status.h"
#include "net/cert/x509_certificate.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/public/proto/sct_audit_report.pb.h"
#include "third_party/boringssl/src/include/openssl/pool.h"
#include "third_party/boringssl/src/include/openssl/sha.h"

namespace network {

namespace {

constexpr int kSendSCTReportTimeoutSeconds = 30;

sct_auditing::SCTWithSourceAndVerifyStatus::SctVerifyStatus
MapSCTVerifyStatusToProtoStatus(net::ct::SCTVerifyStatus status) {
  switch (status) {
    case net::ct::SCTVerifyStatus::SCT_STATUS_NONE:
      return sct_auditing::SCTWithSourceAndVerifyStatus::SctVerifyStatus::
          SCTWithSourceAndVerifyStatus_SctVerifyStatus_NONE;
    case net::ct::SCTVerifyStatus::SCT_STATUS_LOG_UNKNOWN:
      return sct_auditing::SCTWithSourceAndVerifyStatus::SctVerifyStatus::
          SCTWithSourceAndVerifyStatus_SctVerifyStatus_LOG_UNKNOWN;
    case net::ct::SCTVerifyStatus::SCT_STATUS_OK:
      return sct_auditing::SCTWithSourceAndVerifyStatus::SctVerifyStatus::
          SCTWithSourceAndVerifyStatus_SctVerifyStatus_OK;
    case net::ct::SCTVerifyStatus::SCT_STATUS_INVALID_SIGNATURE:
      return sct_auditing::SCTWithSourceAndVerifyStatus::SctVerifyStatus::
          SCTWithSourceAndVerifyStatus_SctVerifyStatus_INVALID_SIGNATURE;
    case net::ct::SCTVerifyStatus::SCT_STATUS_INVALID_TIMESTAMP:
      return sct_auditing::SCTWithSourceAndVerifyStatus::SctVerifyStatus::
          SCTWithSourceAndVerifyStatus_SctVerifyStatus_INVALID_TIMESTAMP;
  }
}

sct_auditing::SCTWithSourceAndVerifyStatus::Source MapSCTOriginToSource(
    net::ct::SignedCertificateTimestamp::Origin origin) {
  switch (origin) {
    case net::ct::SignedCertificateTimestamp::Origin::SCT_EMBEDDED:
      return sct_auditing::SCTWithSourceAndVerifyStatus::Source::
          SCTWithSourceAndVerifyStatus_Source_EMBEDDED;
    case net::ct::SignedCertificateTimestamp::Origin::SCT_FROM_TLS_EXTENSION:
      return sct_auditing::SCTWithSourceAndVerifyStatus::Source::
          SCTWithSourceAndVerifyStatus_Source_TLS_EXTENSION;
    case net::ct::SignedCertificateTimestamp::Origin::SCT_FROM_OCSP_RESPONSE:
      return sct_auditing::SCTWithSourceAndVerifyStatus::Source::
          SCTWithSourceAndVerifyStatus_Source_OCSP_RESPONSE;
    default:
      return sct_auditing::SCTWithSourceAndVerifyStatus::Source::
          SCTWithSourceAndVerifyStatus_Source_SOURCE_UNSPECIFIED;
  }
}

// Records the high-water mark of the cache size (in number of reports).
void RecordSCTAuditingCacheHighWaterMarkMetrics(size_t hwm) {
  base::UmaHistogramCounts1000("Security.SCTAuditing.OptIn.CacheHWM", hwm);
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

// Owns the SimpleURLLoader and runs the callback and then deletes itself when
// the response arrives.
class SimpleURLLoaderOwner {
 public:
  using LoaderDoneCallback =
      base::OnceCallback<void(int /* net_error */,
                              int /* http_response_code */)>;

  SimpleURLLoaderOwner(mojom::URLLoaderFactory* url_loader_factory,
                       std::unique_ptr<SimpleURLLoader> loader,
                       LoaderDoneCallback done_callback)
      : loader_(std::move(loader)), done_callback_(std::move(done_callback)) {
    // We only care about whether the report was successfully received, so we
    // download the headers only.
    // If the loader is destroyed, the callback will be canceled, so using
    // base::Unretained here is safe.
    loader_->DownloadHeadersOnly(
        url_loader_factory,
        base::BindOnce(&SimpleURLLoaderOwner::OnURLLoaderComplete,
                       base::Unretained(this)));
  }

  SimpleURLLoaderOwner(const SimpleURLLoaderOwner&) = delete;
  SimpleURLLoaderOwner& operator=(const SimpleURLLoaderOwner&) = delete;

 private:
  ~SimpleURLLoaderOwner() = default;

  void OnURLLoaderComplete(scoped_refptr<net::HttpResponseHeaders> headers) {
    if (done_callback_) {
      int response_code = 0;
      if (loader_->ResponseInfo() && loader_->ResponseInfo()->headers)
        response_code = loader_->ResponseInfo()->headers->response_code();
      std::move(done_callback_).Run(loader_->NetError(), response_code);
    }
    delete this;
  }

  std::unique_ptr<SimpleURLLoader> loader_;
  LoaderDoneCallback done_callback_;
};

}  // namespace

SCTAuditingCache::SCTAuditingCache(size_t cache_size)
    : cache_(cache_size), cache_size_hwm_(0) {}
SCTAuditingCache::~SCTAuditingCache() {
  RecordSCTAuditingCacheHighWaterMarkMetrics(cache_size_hwm_);
}

void SCTAuditingCache::MaybeEnqueueReport(
    NetworkContext* context,
    const net::HostPortPair& host_port_pair,
    const net::X509Certificate* validated_certificate_chain,
    const net::SignedCertificateTimestampAndStatusList&
        signed_certificate_timestamps) {
  if (!enabled_ || !context->is_sct_auditing_enabled())
    return;

  // Generate the cache key for this report. In order to have the cache
  // deduplicate reports for the same SCTs, we compute the cache key as the
  // hash of the SCTs.
  SHA256_CTX ctx;
  SHA256_Init(&ctx);
  for (const auto& sct : signed_certificate_timestamps) {
    std::string serialized_sct;
    net::ct::EncodeSignedCertificateTimestamp(sct.sct, &serialized_sct);
    SHA256_Update(&ctx, serialized_sct.data(), serialized_sct.size());
  }
  net::SHA256HashValue cache_key;
  SHA256_Final(reinterpret_cast<uint8_t*>(&cache_key), &ctx);

  // Check if the SCTs are already in the cache. This will update the last seen
  // time if they are present in the cache.
  auto it = cache_.Get(cache_key);
  if (it != cache_.end()) {
    RecordSCTAuditingReportDeduplicatedMetrics(true);
    return;
  }
  RecordSCTAuditingReportDeduplicatedMetrics(false);

  // Set the `cache_key` with an null report. If we don't choose to sample these
  // SCTs, then we don't need to store a report as we won't reference it again
  // (and can save on memory usage). If we do choose to sample these SCTs, we
  // then construct the report and move it into the cache entry for `cache_key`.
  cache_.Put(cache_key, nullptr);

  if (base::RandDouble() > sampling_rate_) {
    RecordSCTAuditingReportSampledMetrics(false);
    return;
  }
  RecordSCTAuditingReportSampledMetrics(true);

  // Insert SCTs into cache.
  auto report = std::make_unique<sct_auditing::TLSConnectionReport>();
  auto* connection_context = report->mutable_context();

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

  for (const auto& sct : signed_certificate_timestamps) {
    auto* sct_source_and_status = report->add_included_scts();
    sct_source_and_status->set_status(
        MapSCTVerifyStatusToProtoStatus(sct.status));
    sct_source_and_status->set_source(MapSCTOriginToSource(sct.sct->origin));
    std::string serialized_sct;
    net::ct::EncodeSignedCertificateTimestamp(sct.sct, &serialized_sct);
    sct_source_and_status->set_sct(serialized_sct);
  }

  // Log the size of the report. This only tracks reports that are not dropped
  // due to sampling (as those reports will just be empty).
  RecordSCTAuditingReportSizeMetrics(report->ByteSizeLong());

  cache_.Put(cache_key, std::move(report));

  // Track high-water-mark for the size of the cache.
  if (cache_.size() > cache_size_hwm_)
    cache_size_hwm_ = cache_.size();

  SendReport(cache_key);
}

sct_auditing::TLSConnectionReport* SCTAuditingCache::GetPendingReport(
    const net::SHA256HashValue& cache_key) {
  auto it = cache_.Get(cache_key);
  if (it == cache_.end())
    return nullptr;
  return it->second.get();
}

void SCTAuditingCache::SendReport(const net::SHA256HashValue& cache_key) {
  // Ensure that the URLLoaderFactory is still connected.
  if (!url_loader_factory_ || !url_loader_factory_.is_connected()) {
    // TODO(cthomp): Should this signal to embedder that something has failed?
    return;
  }

  // (1) Get the report from the cache, if it exists.
  auto* report = GetPendingReport(cache_key);
  if (!report) {
    // TODO(crbug.com/1082860): This generally means that the report has been
    // evicted from the cache. We should handle this more gracefully once we
    // implement retrying reports as that will increase the likelihood.
    return;
  }

  // (2) Create a SimpleURLLoader for the request.
  auto report_request = std::make_unique<ResourceRequest>();
  report_request->url = report_uri_;
  report_request->method = "POST";
  report_request->load_flags = net::LOAD_DISABLE_CACHE;
  report_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  auto url_loader = SimpleURLLoader::Create(
      std::move(report_request),
      static_cast<net::NetworkTrafficAnnotationTag>(traffic_annotation_));
  url_loader->SetTimeoutDuration(
      base::TimeDelta::FromSeconds(kSendSCTReportTimeoutSeconds));

  // (3) Serialize the report and attach it to the loader.
  std::string report_data;
  bool ok = report->SerializeToString(&report_data);
  DCHECK(ok);
  url_loader->AttachStringForUpload(report_data, "application/octet-stream");

  // (4) Pass the loader to an owner for its lifetime. This initiates the
  // request and will handle calling `callback` when the request completes
  // (on success or error) or times out.
  // The callback takes a WeakPtr as the SCTAuditingCache or Network Service
  // could be destroyed before the callback triggers.
  auto done_callback = base::BindOnce(&SCTAuditingCache::OnReportComplete,
                                      weak_factory_.GetWeakPtr(), cache_key);
  new SimpleURLLoaderOwner(url_loader_factory_.get(), std::move(url_loader),
                           std::move(done_callback));
}

void SCTAuditingCache::OnReportComplete(const net::SHA256HashValue& cache_key,
                                        int net_error,
                                        int http_response_code) {
  // TODO(crbug.com/1082860): Mark report as complete on success, handle retries
  // on failures. For now we empty the cache entry to save space once it has
  // been successfully sent.
  bool success = net_error == net::OK && http_response_code == net::HTTP_OK;
  if (success) {
    if (GetPendingReport(cache_key))
      cache_.Put(cache_key, nullptr);
  }
  RecordSCTAuditingReportSucceededMetrics(success);
}

void SCTAuditingCache::ClearCache() {
  cache_.Clear();
}

}  // namespace network
