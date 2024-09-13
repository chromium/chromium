// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_report_network_sender.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/overloaded.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/values.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "content/browser/attribution_reporting/aggregatable_debug_report.h"
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
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(Status)
enum class Status {
  kOk = 0,
  // Corresponds to a non-zero NET_ERROR.
  kInternalError = 1,
  // Corresponds to a non-200 HTTP response code from the reporting endpoint.
  kExternalError = 2,
  kMaxValue = kExternalError
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/attribution_reporting/enums.xml:ConversionReportStatus)

template <typename T>
void NetworkHistogram(std::string_view suffix,
                      void (*hist_func)(const std::string&, T value),
                      bool is_debug_report,
                      std::optional<bool> has_trigger_context_id,
                      T value) {
  if (is_debug_report) {
    hist_func(base::StrCat({"Conversions.DebugReport.", suffix}), value);
  } else {
    hist_func(base::StrCat({"Conversions.", suffix}), value);
    if (has_trigger_context_id.has_value()) {
      if (*has_trigger_context_id) {
        hist_func(base::StrCat({"Conversions.ContextID.", suffix}), value);
      } else {
        hist_func(base::StrCat({"Conversions.NoContextID.", suffix}), value);
      }
    }
  }
}

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

  if (!is_debug_report) {
    switch (report.GetReportType()) {
      case AttributionReport::Type::kEventLevel:
        base::UmaHistogramCounts1000(
            "Conversions.EventLevelReport.ReportBodySize", body.size());
        break;
      case AttributionReport::Type::kAggregatableAttribution:
      case AttributionReport::Type::kNullAggregatable:
        base::UmaHistogramCounts10000(
            "Conversions.AggregatableReport.ReportBodySize", body.size());
        break;
    }
  }

  url::Origin origin(report.reporting_origin());
  SendReport(std::move(url), std::move(origin), body,
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
      std::move(url), std::move(origin), body,
      base::BindOnce(&AttributionReportNetworkSender::OnVerboseDebugReportSent,
                     base::Unretained(this),
                     base::BindOnce(std::move(callback), std::move(report))));
}

void AttributionReportNetworkSender::SendReport(
    AggregatableDebugReport report,
    base::Value::Dict report_body,
    AggregatableDebugReportSentCallback callback) {
  GURL url(report.ReportUrl());
  url::Origin origin(report.reporting_origin());
  std::string body = SerializeAttributionJson(report_body);
  SendReport(std::move(url), std::move(origin), body,
             base::BindOnce(
                 &AttributionReportNetworkSender::OnAggregatableDebugReportSent,
                 base::Unretained(this),
                 base::BindOnce(std::move(callback), std::move(report),
                                std::move(report_body))));
}

void AttributionReportNetworkSender::SendReport(GURL url,
                                                url::Origin origin,
                                                const std::string& body,
                                                UrlLoaderCallback callback) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = std::move(url);
  resource_request->method = net::HttpRequestHeaders::kPostMethod;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->mode = network::mojom::RequestMode::kSameOrigin;
  resource_request->request_initiator = std::move(origin);
  resource_request->load_flags =
      net::LOAD_DISABLE_CACHE | net::LOAD_BYPASS_CACHE;
  resource_request->trusted_params = network::ResourceRequest::TrustedParams();
  resource_request->trusted_params->isolation_info =
      net::IsolationInfo::CreateTransient();

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
            "Debug reports include data related to attribution source or "
            "trigger registration failures."
          destination:OTHER
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature can be controlled via the 'Ad measurement' setting "
            "in the 'Ad privacy' section of 'Privacy and Security'."
          chrome_policy {
            PrivacySandboxAdMeasurementEnabled {
              PrivacySandboxAdMeasurementEnabled: false
            }
          }
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
  // Since net errors are always negative and HTTP errors are always positive,
  // it is fine to combine these in a single histogram.
  int response_or_net_error = internal_ok ? response_code : net_error;
  std::optional<bool> retry_succeed =
      loader->GetNumRetries() > 0
          ? std::make_optional<bool>(status == Status::kOk)
          : std::nullopt;

  std::optional<bool> has_trigger_context_id;

  absl::visit(
      base::Overloaded{
          [&](const AttributionReport::EventLevelData&) {
            NetworkHistogram("ReportStatusEventLevel",
                             &base::UmaHistogramEnumeration, is_debug_report,
                             has_trigger_context_id, status);
            NetworkHistogram("HttpResponseOrNetErrorCodeEventLevel",
                             &base::UmaHistogramSparse, is_debug_report,
                             has_trigger_context_id, response_or_net_error);
            if (retry_succeed.has_value()) {
              NetworkHistogram("ReportRetrySucceedEventLevel",
                               &base::UmaHistogramBoolean, is_debug_report,
                               has_trigger_context_id, *retry_succeed);
            }
          },
          [&](const AttributionReport::AggregatableAttributionData& data) {
            has_trigger_context_id =
                data.common_data.aggregatable_trigger_config
                    .trigger_context_id()
                    .has_value();
            NetworkHistogram("ReportStatusAggregatable",
                             &base::UmaHistogramEnumeration, is_debug_report,
                             has_trigger_context_id, status);
            NetworkHistogram("HttpResponseOrNetErrorCodeAggregatable",
                             &base::UmaHistogramSparse, is_debug_report,
                             has_trigger_context_id, response_or_net_error);
            if (retry_succeed.has_value()) {
              NetworkHistogram("ReportRetrySucceedAggregatable",
                               &base::UmaHistogramBoolean, is_debug_report,
                               has_trigger_context_id, *retry_succeed);
            }
          },
          [&](const AttributionReport::NullAggregatableData& data) {
            has_trigger_context_id =
                data.common_data.aggregatable_trigger_config
                    .trigger_context_id()
                    .has_value();
            NetworkHistogram("ReportStatusAggregatableNull",
                             &base::UmaHistogramEnumeration, is_debug_report,
                             has_trigger_context_id, status);
            NetworkHistogram("HttpResponseOrNetErrorCodeAggregatableNull",
                             &base::UmaHistogramSparse, is_debug_report,
                             has_trigger_context_id, response_or_net_error);
          },
      },
      report.data());

  loaders_in_progress_.erase(it);

  if (status == Status::kOk) {
    std::move(sent_callback)
        .Run(report,
             SendResult::Sent(SendResult::Sent::Result::kSent, response_code));
  } else {
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
    std::move(sent_callback)
        .Run(report,
             SendResult::Sent(should_retry
                                  ? SendResult::Sent::Result::kTransientFailure
                                  : SendResult::Sent::Result::kFailure,
                              net_error));
  }
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

void AttributionReportNetworkSender::OnAggregatableDebugReportSent(
    base::OnceCallback<void(int status)> callback,
    UrlLoaderList::iterator it,
    scoped_refptr<net::HttpResponseHeaders> headers) {
  // HTTP statuses are positive; network errors are negative.
  int status = headers ? headers->response_code() : (*it)->NetError();

  // Since net errors are always negative and HTTP errors are always positive,
  // it is fine to combine these in a single histogram.
  base::UmaHistogramSparse(
      "Conversions.AggregatableDebugReport.HttpResponseOrNetErrorCode", status);

  loaders_in_progress_.erase(it);
  std::move(callback).Run(status);
}

}  // namespace content
