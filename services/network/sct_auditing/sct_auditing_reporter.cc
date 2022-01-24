// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/sct_auditing/sct_auditing_reporter.h"

#include "base/callback.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace network {

namespace {

constexpr int kSendSCTReportTimeoutSeconds = 30;

// Overrides the initial retry delay in SCTAuditingReporter::kBackoffPolicy if
// not nullopt.
absl::optional<base::TimeDelta> g_retry_delay_for_testing = absl::nullopt;

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
}

SCTAuditingReporter::~SCTAuditingReporter() = default;

void SCTAuditingReporter::Start() {
  // Informing the backoff entry of a success will force it to use the initial
  // delay (and jitter) for the first attempt. Otherwise, ShouldRejectRequest()
  // will return `true` despite the policy specifying
  // `always_use_initial_delay = true`.
  backoff_entry_->InformOfRequest(true);

  // Start sending the report.
  ScheduleReport();
}

void SCTAuditingReporter::SetRetryDelayForTesting(
    absl::optional<base::TimeDelta> delay) {
  g_retry_delay_for_testing = delay;
}

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

}  // namespace network
