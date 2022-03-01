// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/sct_auditing/sct_auditing_reporter.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/json/json_reader.h"
#include "base/json/values_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/network_context.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace network {

namespace {

constexpr int kSendSCTReportTimeoutSeconds = 30;

// The number of bits of an SCT leaf hash sent in a lookup query.
constexpr size_t kHashdanceHashPrefixLength = 20;

// The maximum allowed size for the lookup query response. 4MB.
constexpr size_t kMaxLookupResponseSize = 1024 * 1024 * 4;

// HashdanceMetadata JSON serialization keys.
constexpr char kLeafHashKey[] = "leaf_hash";
constexpr char kIssuedKey[] = "issued";
constexpr char kLogIdKey[] = "log_id";
constexpr char kLogMMDKey[] = "log_mmd";
constexpr char kCertificateExpiry[] = "cert_expiry";

// Hashdance server response JSON keys.
constexpr char kLookupStatusKey[] = "responseStatus";
constexpr char kLookupHashSuffixKey[] = "hashSuffix";
constexpr char kLookupLogStatusKey[] = "logStatus";
constexpr char kLookupLogIdKey[] = "logId";
constexpr char kLookupIngestedUntilKey[] = "ingestedUntil";
constexpr char kLookupTimestampKey[] = "now";

constexpr char kStatusOK[] = "OK";

// Overrides the initial retry delay in SCTAuditingReporter::kBackoffPolicy if
// not nullopt.
absl::optional<base::TimeDelta> g_retry_delay_for_testing = absl::nullopt;

void RecordLookupQueryResult(SCTAuditingReporter::LookupQueryResult result) {
  base::UmaHistogramEnumeration("Security.SCTAuditing.OptOut.LookupQueryResult",
                                result);
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

std::string TruncatePrefix(base::span<char> bytes, size_t bits_number) {
  CHECK_GT(bits_number, 0u);
  CHECK_GE(bytes.size(), bits_number / 8);
  // Calculate the number of bytes needed to store |bits_number| bits.
  size_t bytes_number = (bits_number + 7) / 8;
  std::string truncated(bytes.begin(), bytes.begin() + bytes_number);

  // Mask the last byte so only the last |remainder_bits| are sent.  E.g. if
  // |remainder_bits| is 2, then the code below will mask the last byte with
  // 0b11000000.
  size_t remainder_bits = bits_number % 8;
  if (remainder_bits == 0) {
    remainder_bits = 8;
  }
  truncated[truncated.size() - 1] &= ~(0xff >> remainder_bits);
  return truncated;
}

std::string TruncateSuffix(base::span<char> bytes, size_t bits_number) {
  CHECK_GT(bits_number, 0u);
  CHECK_GE(bytes.size(), bits_number / 8);

  // For the suffix, the server will always round up to the nearest number of
  // bytes. Thus, truncate |bytes| to the nearest number of bytes that can hold
  // |bits_number| bits.
  size_t bytes_number = (bits_number + 7) / 8;
  std::string truncated(bytes.begin() + bytes_number - 1, bytes.end());
  return truncated;
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
constexpr int kMaxRetries = 15;

// static
absl::optional<SCTAuditingReporter::SCTHashdanceMetadata>
SCTAuditingReporter::SCTHashdanceMetadata::FromValue(const base::Value& value) {
  if (!value.is_dict()) {
    return absl::nullopt;
  }

  const std::string* encoded_leaf_hash = value.FindStringKey(kLeafHashKey);
  const absl::optional<base::Time> issued =
      base::ValueToTime(value.FindKey(kIssuedKey));
  const std::string* encoded_log_id = value.FindStringKey(kLogIdKey);
  const absl::optional<base::TimeDelta> log_mmd =
      base::ValueToTimeDelta(value.FindKey(kLogMMDKey));
  const absl::optional<base::Time> certificate_expiry =
      base::ValueToTime(value.FindKey(kCertificateExpiry));
  if (!encoded_leaf_hash || !encoded_log_id || !log_mmd || !issued ||
      !certificate_expiry) {
    return absl::nullopt;
  }

  SCTAuditingReporter::SCTHashdanceMetadata sct_hashdance_metadata;
  if (!base::Base64Decode(*encoded_leaf_hash,
                          &sct_hashdance_metadata.leaf_hash)) {
    return absl::nullopt;
  }
  if (!base::Base64Decode(*encoded_log_id, &sct_hashdance_metadata.log_id)) {
    return absl::nullopt;
  }
  sct_hashdance_metadata.log_mmd = std::move(*log_mmd);
  sct_hashdance_metadata.issued = std::move(*issued);
  sct_hashdance_metadata.certificate_expiry = std::move(*certificate_expiry);
  return sct_hashdance_metadata;
}

SCTAuditingReporter::SCTHashdanceMetadata::SCTHashdanceMetadata() = default;
SCTAuditingReporter::SCTHashdanceMetadata::~SCTHashdanceMetadata() = default;
SCTAuditingReporter::SCTHashdanceMetadata::SCTHashdanceMetadata(
    SCTHashdanceMetadata&&) = default;
SCTAuditingReporter::SCTHashdanceMetadata&
SCTAuditingReporter::SCTHashdanceMetadata::operator=(SCTHashdanceMetadata&&) =
    default;

base::Value SCTAuditingReporter::SCTHashdanceMetadata::ToValue() const {
  base::DictionaryValue value;
  value.SetStringKey(
      kLeafHashKey,
      base::Base64Encode(base::as_bytes(base::make_span(leaf_hash))));
  value.SetKey(kIssuedKey, base::TimeToValue(issued));
  value.SetStringKey(
      kLogIdKey, base::Base64Encode(base::as_bytes(base::make_span(log_id))));
  value.SetKey(kLogMMDKey, base::TimeDeltaToValue(log_mmd));
  value.SetKey(kCertificateExpiry, base::TimeToValue(certificate_expiry));
  return std::move(value);
}

// static
void SCTAuditingReporter::SetRetryDelayForTesting(
    absl::optional<base::TimeDelta> delay) {
  g_retry_delay_for_testing = delay;
}

SCTAuditingReporter::SCTAuditingReporter(
    NetworkContext* owner_network_context,
    net::HashValue reporter_key,
    std::unique_ptr<sct_auditing::SCTClientReport> report,
    bool is_hashdance,
    absl::optional<SCTHashdanceMetadata> sct_hashdance_metadata,
    mojom::SCTAuditingConfigurationPtr configuration,
    mojom::URLLoaderFactory* url_loader_factory,
    ReporterUpdatedCallback update_callback,
    ReporterDoneCallback done_callback,
    std::unique_ptr<net::BackoffEntry> persisted_backoff_entry)
    : owner_network_context_(owner_network_context),
      reporter_key_(reporter_key),
      report_(std::move(report)),
      is_hashdance_(is_hashdance),
      sct_hashdance_metadata_(std::move(sct_hashdance_metadata)),
      configuration_(std::move(configuration)),
      update_callback_(std::move(update_callback)),
      done_callback_(std::move(done_callback)),
      max_retries_(kMaxRetries) {
  // Clone the URLLoaderFactory to avoid any dependencies on its lifetime. The
  // Reporter instance can maintain its own copy.
  // Relatively few Reporters are expected to exist at a time (due to sampling
  // and deduplication), so some cost of copying is reasonable. If more
  // optimization is needed, this could potentially use a mojo::SharedRemote
  // or a WrappedPendingSharedURLLoaderFactory instead.
  url_loader_factory->Clone(
      url_loader_factory_remote_.BindNewPipeAndPassReceiver());

  // Override the retry delay if set by tests.
  backoff_policy_ = kDefaultBackoffPolicy;
  if (g_retry_delay_for_testing) {
    backoff_policy_.initial_delay_ms =
        g_retry_delay_for_testing->InMilliseconds();
  }

  // `persisted_backoff_entry` is only non-null when persistence is enabled and
  // this SCTAuditingReporter is being created from a reporter that had been
  // persisted to disk.
  if (persisted_backoff_entry) {
    backoff_entry_ = std::move(persisted_backoff_entry);
  } else {
    backoff_entry_ = std::make_unique<net::BackoffEntry>(&backoff_policy_);
    // Informing the backoff entry of a success will force it to use the initial
    // delay (and jitter) for the first attempt. Otherwise,
    // ShouldRejectRequest() will return `true` despite the policy specifying
    // `always_use_initial_delay = true`.
    backoff_entry_->InformOfRequest(true);
  }
}

SCTAuditingReporter::~SCTAuditingReporter() = default;

void SCTAuditingReporter::Start() {
  if (!is_hashdance_) {
    ScheduleRequestWithBackoff(base::BindOnce(&SCTAuditingReporter::SendReport,
                                              weak_factory_.GetWeakPtr()),
                               base::TimeDelta());
    return;
  }

  // Entrypoint for checking whether the max-reports limit has been reached.
  // This should only get called once for the lifetime of the Reporter.
  // TODO(crbug.com/1144205): Once reports are persisted to disk, the Reporter
  // state should include whether it has been "counted" yet, otherwise if a
  // Reporter gets persisted and restored many times it would cause the report
  // cap to trigger. This can likely just be a boolean flag on the Reporter and
  // the persisted state -- if `true`, this check (and incrementing the report
  // count) can be skipped.
  owner_network_context_->CanSendSCTAuditingReport(
      base::BindOnce(&SCTAuditingReporter::OnCheckReportAllowedStatusComplete,
                     weak_factory_.GetWeakPtr()));
}

void SCTAuditingReporter::OnCheckReportAllowedStatusComplete(bool allowed) {
  if (!allowed) {
    // The maximum report cap has already been reached. Notify the handler that
    // this Reporter is done. This will delete `this`, so do not add code after
    // this point.
    std::move(done_callback_).Run(reporter_key_);
    return;
  }

  // Calculate an estimated minimum delay after which the log is expected to
  // have been ingested by the server.
  base::TimeDelta random_delay = base::Seconds(base::RandInt(
      0, configuration_->log_max_ingestion_random_delay.InSeconds()));
  base::TimeDelta delay = sct_hashdance_metadata_->issued +
                          sct_hashdance_metadata_->log_mmd +
                          configuration_->log_expected_ingestion_delay +
                          random_delay - base::Time::Now();
  ScheduleRequestWithBackoff(
      base::BindOnce(&SCTAuditingReporter::SendLookupQuery,
                     weak_factory_.GetWeakPtr()),
      delay);
}

void SCTAuditingReporter::ScheduleRequestWithBackoff(base::OnceClosure request,
                                                     base::TimeDelta delay) {
  if (base::FeatureList::IsEnabled(features::kSCTAuditingRetryReports) &&
      backoff_entry_->ShouldRejectRequest()) {
    delay = std::max(backoff_entry_->GetTimeUntilRelease(), delay);
  }
  if (delay.is_positive()) {
    // TODO(crbug.com/1199827): Investigate if explicit task traits should be
    // used for these tasks (e.g., BEST_EFFORT and SKIP_ON_SHUTDOWN).
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, std::move(request), delay);
    return;
  }
  std::move(request).Run();
}

void SCTAuditingReporter::SendLookupQuery() {
  DCHECK(url_loader_factory_remote_);
  // Create a SimpleURLLoader for the request.
  auto report_request = std::make_unique<ResourceRequest>();

  // Serialize the URL parameters.
  std::string hash_prefix = TruncatePrefix(sct_hashdance_metadata_->leaf_hash,
                                           kHashdanceHashPrefixLength);
  report_request->url = GURL(base::ReplaceStringPlaceholders(
      configuration_->hashdance_lookup_uri.spec(),
      {
          base::NumberToString(kHashdanceHashPrefixLength),
          base::HexEncode(base::as_bytes(base::make_span(hash_prefix))),
      },
      /*offsets=*/nullptr));
  report_request->method = "GET";
  report_request->load_flags = net::LOAD_DISABLE_CACHE;
  report_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  url_loader_ = SimpleURLLoader::Create(
      std::move(report_request),
      static_cast<net::NetworkTrafficAnnotationTag>(
          configuration_->hashdance_traffic_annotation));
  url_loader_->SetTimeoutDuration(base::Seconds(kSendSCTReportTimeoutSeconds));
  // Retry is handled by SCTAuditingReporter.
  url_loader_->SetRetryOptions(0, SimpleURLLoader::RETRY_NEVER);

  // If the loader is destroyed, the callback will be canceled, so using
  // base::Unretained here is safe.
  url_loader_->DownloadToString(
      url_loader_factory_remote_.get(),
      base::BindOnce(&SCTAuditingReporter::OnSendLookupQueryComplete,
                     base::Unretained(this)),
      kMaxLookupResponseSize);
}

void SCTAuditingReporter::OnSendLookupQueryComplete(
    std::unique_ptr<std::string> response_body) {
  int response_code = 0;
  if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers) {
    response_code = url_loader_->ResponseInfo()->headers->response_code();
  }
  bool success = url_loader_->NetError() == net::OK &&
                 response_code == net::HTTP_OK && response_body;
  if (!success) {
    RecordLookupQueryResult(LookupQueryResult::kHTTPError);
    MaybeRetryRequest();
    return;
  }

  absl::optional<base::Value> result = base::JSONReader::Read(*response_body);
  if (!result) {
    RecordLookupQueryResult(LookupQueryResult::kInvalidJson);
    MaybeRetryRequest();
    return;
  }

  const std::string* status = result->FindStringKey(kLookupStatusKey);
  if (!status) {
    RecordLookupQueryResult(LookupQueryResult::kInvalidJson);
    MaybeRetryRequest();
    return;
  }
  if (*status != kStatusOK) {
    RecordLookupQueryResult(LookupQueryResult::kStatusNotOk);
    MaybeRetryRequest();
    return;
  }

  const std::string* server_timestamp_string =
      result->FindStringKey(kLookupTimestampKey);
  if (!server_timestamp_string) {
    RecordLookupQueryResult(LookupQueryResult::kInvalidJson);
    MaybeRetryRequest();
    return;
  }

  base::Time server_timestamp;
  if (!base::Time::FromUTCString(server_timestamp_string->c_str(),
                                 &server_timestamp)) {
    RecordLookupQueryResult(LookupQueryResult::kInvalidJson);
    MaybeRetryRequest();
    return;
  }

  if (sct_hashdance_metadata_->certificate_expiry < server_timestamp) {
    // The certificate has expired. Do not report.
    RecordLookupQueryResult(LookupQueryResult::kCertificateExpired);
    std::move(done_callback_).Run(reporter_key_);
    return;
  }

  // Find the corresponding log entry.
  const base::Value* logs = result->FindListKey(kLookupLogStatusKey);
  if (!logs) {
    RecordLookupQueryResult(LookupQueryResult::kInvalidJson);
    MaybeRetryRequest();
    return;
  }

  const base::Value* found_log = nullptr;
  for (const auto& log : logs->GetListDeprecated()) {
    const std::string* encoded_log_id = log.FindStringKey(kLookupLogIdKey);
    std::string log_id;
    if (!encoded_log_id || !base::Base64Decode(*encoded_log_id, &log_id)) {
      RecordLookupQueryResult(LookupQueryResult::kInvalidJson);
      MaybeRetryRequest();
      return;
    }
    if (log_id == sct_hashdance_metadata_->log_id) {
      found_log = &log;
      break;
    }
  }
  if (!found_log) {
    // We could not find the SCT's log. Maybe it's a new log that the server
    // doesn't know about yet, schedule a retry.
    RecordLookupQueryResult(LookupQueryResult::kLogNotFound);
    MaybeRetryRequest();
    return;
  }

  const std::string* ingested_until_string =
      found_log->FindStringKey(kLookupIngestedUntilKey);
  if (!ingested_until_string) {
    RecordLookupQueryResult(LookupQueryResult::kInvalidJson);
    MaybeRetryRequest();
    return;
  }

  base::Time ingested_until;
  if (!base::Time::FromString(ingested_until_string->c_str(),
                              &ingested_until)) {
    RecordLookupQueryResult(LookupQueryResult::kInvalidJson);
    MaybeRetryRequest();
    return;
  }

  if (sct_hashdance_metadata_->issued > ingested_until) {
    // The log has not yet ingested this SCT. Schedule a retry.
    RecordLookupQueryResult(LookupQueryResult::kLogNotYetIngested);
    MaybeRetryRequest();
    return;
  }

  const base::Value* suffix_value = result->FindListKey(kLookupHashSuffixKey);
  if (!suffix_value) {
    RecordLookupQueryResult(LookupQueryResult::kInvalidJson);
    MaybeRetryRequest();
    return;
  }

  // Calculate the hash suffix and wrap it in a base::Value for a simpler
  // comparison without having to convert every value in the |suffix_value|.
  std::string hash_suffix = TruncateSuffix(sct_hashdance_metadata_->leaf_hash,
                                           kHashdanceHashPrefixLength);
  hash_suffix =
      base::Base64Encode(base::as_bytes(base::make_span(hash_suffix)));
  base::Value hash_suffix_value(std::move(hash_suffix));
  const auto suffixes = suffix_value->GetListDeprecated();
  // TODO(nsatragno): it would be neat if the backend returned a sorted list and
  // we could binary search it instead.
  if (std::find(suffixes.begin(), suffixes.end(), hash_suffix_value) !=
      suffixes.end()) {
    // Found the SCT in the suffix list, all done.
    RecordLookupQueryResult(LookupQueryResult::kSCTSuffixFound);
    std::move(done_callback_).Run(reporter_key_);
    return;
  }

  // The server does not know about this SCT, and it should. Notify the
  // embedder and start sending the full report.
  owner_network_context_->OnNewSCTAuditingReportSent();
  RecordLookupQueryResult(LookupQueryResult::kSCTSuffixNotFound);
  SendReport();
}

void SCTAuditingReporter::SendReport() {
  DCHECK(url_loader_factory_remote_);

  // Create a SimpleURLLoader for the request.
  auto report_request = std::make_unique<ResourceRequest>();
  report_request->url = configuration_->report_uri;
  report_request->method = "POST";
  report_request->load_flags = net::LOAD_DISABLE_CACHE;
  report_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  url_loader_ = SimpleURLLoader::Create(
      std::move(report_request), static_cast<net::NetworkTrafficAnnotationTag>(
                                     configuration_->traffic_annotation));
  url_loader_->SetTimeoutDuration(base::Seconds(kSendSCTReportTimeoutSeconds));
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

  if (!success) {
    MaybeRetryRequest();
    return;
  }
  // Report succeeded.
  if (backoff_entry_->failure_count() == 0) {
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

void SCTAuditingReporter::MaybeRetryRequest() {
  if (base::FeatureList::IsEnabled(features::kSCTAuditingRetryReports)) {
    if (backoff_entry_->failure_count() >= max_retries_) {
      // Retry limit reached.
      ReportSCTAuditingCompletionStatusMetrics(
          CompletionStatus::kRetriesExhausted);

      // Notify the Cache that this Reporter is done. This will delete |this|,
      // so do not add code after this point.
      std::move(done_callback_).Run(reporter_key_);
      return;
    }
    // Schedule a retry and alert the SCTAuditingHandler to trigger a write so
    // it can persist the updated backoff entry.
    backoff_entry_->InformOfRequest(false);
    update_callback_.Run();
    Start();
    return;
  }
  // Notify the Cache that this Reporter is done. This will delete |this|,
  // so do not add code after this point.
  std::move(done_callback_).Run(reporter_key_);
  return;
}

}  // namespace network
