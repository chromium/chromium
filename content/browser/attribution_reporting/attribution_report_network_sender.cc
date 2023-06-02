// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_report_network_sender.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "content/browser/attribution_reporting/attribution_debug_report.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_utils.h"
#include "content/browser/attribution_reporting/send_result.h"
#include "net/base/isolation_info.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class Status {
  kOk = 0,
  // Corresponds to a non-zero NET_ERROR.
  kInternalError = 1,
  // Corresponds to a non-200 HTTP response code from the reporting endpoint.
  kExternalError = 2,
  kMaxValue = kExternalError
};

}  // namespace

AttributionReportNetworkSender::AttributionReportNetworkSender(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(std::move(url_loader_factory)) {
  DCHECK(url_loader_factory_);
}

AttributionReportNetworkSender::~AttributionReportNetworkSender() = default;

void AttributionReportNetworkSender::SendReport(
    AttributionReport report,
    bool is_debug_report,
    ReportSentCallback sent_callback) {
  GURL url = report.ReportURL(is_debug_report);
  std::string body = SerializeAttributionJson(report.ReportBody());
  net::HttpRequestHeaders headers;
  report.PopulateAdditionalHeaders(headers);

  url::Origin origin(report.GetReportingOrigin());
  SendReport(std::move(url), std::move(origin), body, std::move(headers),
             base::BindOnce(&AttributionReportNetworkSender::OnReportSent,
                            base::Unretained(this), std::move(report),
                            is_debug_report, std::move(sent_callback)));
}

void AttributionReportNetworkSender::SendReport(
    AttributionDebugReport report,
    DebugReportSentCallback callback) {
  GURL url(report.ReportUrl());
  url::Origin origin(report.reporting_origin());
  std::string body = SerializeAttributionJson(report.ReportBody());
  SendReport(
      std::move(url), std::move(origin), body, net::HttpRequestHeaders(),
      base::BindOnce(&AttributionReportNetworkSender::OnVerboseDebugReportSent,
                     base::Unretained(this),
                     base::BindOnce(std::move(callback), std::move(report))));
}

void AttributionReportNetworkSender::SendReport(GURL url,
                                                url::Origin origin,
                                                const std::string& body,
                                                net::HttpRequestHeaders headers,
                                                UrlLoaderCallback callback) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = std::move(url);
  resource_request->headers = std::move(headers);
  resource_request->method = net::HttpRequestHeaders::kPostMethod;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->mode = network::mojom::RequestMode::kSameOrigin;
  resource_request->request_initiator = std::move(origin);
  resource_request->load_flags =
      net::LOAD_DISABLE_CACHE | net::LOAD_BYPASS_CACHE;
  resource_request->trusted_params = network::ResourceRequest::TrustedParams();
  resource_request->trusted_params->isolation_info =
      net::IsolationInfo::CreateTransient();

  // TODO(https://crbug.com/1058018): Update the "policy" field in the traffic
  // annotation when a setting to disable the API is properly
  // surfaced/implemented.
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("conversion_measurement_report", R"(
        semantics {
          sender: "Attribution Reporting API"
          description:
            "The Attribution Reporting API supports measurement of clicks and "
            "views with event-level and aggregatable reports without using "
            "cross-site persistent identifiers like third-party cookies."
          trigger:
            "When a triggered attribution has become eligible for reporting "
            "or when an attribution source or trigger registration has failed "
            "and is eligible for error reporting."
          data:
            "Event-level reports include a high-entropy identifier declared "
            "by the site on which the user clicked on or viewed a source and "
            "a noisy low-entropy data value declared on the destination site."
            "Aggregatable reports include encrypted information generated "
            "from both source-side and trigger-side registrations."
            "Verbose debug reports include data related to attribution source "
            "or trigger registration failures."
          destination:OTHER
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be disabled by settings."
          policy_exception_justification: "Not implemented."
        })");

  auto simple_url_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  network::SimpleURLLoader* simple_url_loader_ptr = simple_url_loader.get();

  auto it = loaders_in_progress_.insert(loaders_in_progress_.begin(),
                                        std::move(simple_url_loader));
  simple_url_loader_ptr->SetTimeoutDuration(base::Seconds(30));

  simple_url_loader_ptr->AttachStringForUpload(body, "application/json");

  // Retry once on network change. A network change during DNS resolution
  // results in a DNS error rather than a network change error, so retry in
  // those cases as well.
  int retry_mode = network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE |
                   network::SimpleURLLoader::RETRY_ON_NAME_NOT_RESOLVED;
  simple_url_loader_ptr->SetRetryOptions(/*max_retries=*/1, retry_mode);

  simple_url_loader_ptr->DownloadHeadersOnly(
      url_loader_factory_.get(),
      base::BindOnce(std::move(callback), std::move(it)));
}

void AttributionReportNetworkSender::OnReportSent(
    const AttributionReport& report,
    bool is_debug_report,
    ReportSentCallback sent_callback,
    UrlLoaderList::iterator it,
    scoped_refptr<net::HttpResponseHeaders> headers) {
  network::SimpleURLLoader* loader = it->get();

  // Consider a non-200 HTTP code as a non-internal error.
  int net_error = loader->NetError();
  bool internal_ok =
      net_error == net::OK || net_error == net::ERR_HTTP_RESPONSE_CODE_FAILURE;

  int response_code = headers ? headers->response_code() : -1;
  bool external_ok = response_code >= 200 && response_code <= 299;
  Status status = internal_ok && external_ok ? Status::kOk
                  : !internal_ok             ? Status::kInternalError
                                             : Status::kExternalError;

  const char* status_metric = nullptr;
  const char* http_response_or_net_error_code_metric = nullptr;
  const char* retry_succeed_metric = nullptr;

  switch (report.GetReportType()) {
    case AttributionReport::Type::kEventLevel:
      status_metric = is_debug_report
                          ? "Conversions.DebugReport.ReportStatusEventLevel"
                          : "Conversions.ReportStatusEventLevel";
      http_response_or_net_error_code_metric =
          is_debug_report
              ? "Conversions.DebugReport.HttpResponseOrNetErrorCodeEventLevel"
              : "Conversions.HttpResponseOrNetErrorCodeEventLevel";
      retry_succeed_metric =
          is_debug_report
              ? "Conversions.DebugReport.ReportRetrySucceedEventLevel"
              : "Conversions.ReportRetrySucceedEventLevel";
      break;
    case AttributionReport::Type::kAggregatableAttribution:
      status_metric = is_debug_report
                          ? "Conversions.DebugReport.ReportStatusAggregatable"
                          : "Conversions.ReportStatusAggregatable";
      http_response_or_net_error_code_metric =
          is_debug_report
              ? "Conversions.DebugReport.HttpResponseOrNetErrorCodeAggregatable"
              : "Conversions.HttpResponseOrNetErrorCodeAggregatable";
      retry_succeed_metric =
          is_debug_report
              ? "Conversions.DebugReport.ReportRetrySucceedAggregatable"
              : "Conversions.ReportRetrySucceedAggregatable";
      break;
    case AttributionReport::Type::kNullAggregatable:
      break;
  }

  if (status_metric) {
    base::UmaHistogramEnumeration(status_metric, status);

    // Since net errors are always negative and HTTP errors are always positive,
    // it is fine to combine these in a single histogram.
    base::UmaHistogramSparse(http_response_or_net_error_code_metric,
                             internal_ok ? response_code : net_error);

    if (loader->GetNumRetries() > 0) {
      base::UmaHistogramBoolean(retry_succeed_metric, status == Status::kOk);
    }
  }

  loaders_in_progress_.erase(it);

  // Retry reports that have not received headers and failed with one of the
  // specified error codes. These codes are chosen from the
  // "Conversions.Report.HttpResponseOrNetErrorCode" histogram. HTTP errors
  // should not be retried to prevent over requesting servers.
  bool should_retry =
      !headers && (net_error == net::ERR_INTERNET_DISCONNECTED ||
                   net_error == net::ERR_NAME_NOT_RESOLVED ||
                   net_error == net::ERR_TIMED_OUT ||
                   net_error == net::ERR_CONNECTION_TIMED_OUT ||
                   net_error == net::ERR_CONNECTION_ABORTED ||
                   net_error == net::ERR_CONNECTION_RESET);

  SendResult::Status report_status =
      (status == Status::kOk)
          ? SendResult::Status::kSent
          : (should_retry ? SendResult::Status::kTransientFailure
                          : SendResult::Status::kFailure);

  std::move(sent_callback)
      .Run(report, SendResult(report_status, net_error,
                              headers ? headers->response_code() : 0));
}

void AttributionReportNetworkSender::OnVerboseDebugReportSent(
    base::OnceCallback<void(int status)> callback,
    UrlLoaderList::iterator it,
    scoped_refptr<net::HttpResponseHeaders> headers) {
  // HTTP statuses are positive; network errors are negative.
  int status = headers ? headers->response_code() : (*it)->NetError();

  // Since net errors are always negative and HTTP errors are always positive,
  // it is fine to combine these in a single histogram.
  base::UmaHistogramSparse(
      "Conversions.VerboseDebugReport.HttpResponseOrNetErrorCode", status);

  loaders_in_progress_.erase(it);
  std::move(callback).Run(status);
}

}  // namespace content
