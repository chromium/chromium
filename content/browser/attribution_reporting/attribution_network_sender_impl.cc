// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_network_sender_impl.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_utils.h"
#include "content/browser/attribution_reporting/send_result.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/isolation_info.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

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

AttributionNetworkSenderImpl::AttributionNetworkSenderImpl(
    StoragePartition* storage_partition)
    : storage_partition_(storage_partition) {}

AttributionNetworkSenderImpl::~AttributionNetworkSenderImpl() = default;

void AttributionNetworkSenderImpl::SendReport(
    AttributionReport report,
    ReportSentCallback sent_callback) {
  // The browser process URLLoaderFactory is not created by default, so don't
  // create it until it is directly needed.
  if (!url_loader_factory_) {
    url_loader_factory_ =
        storage_partition_->GetURLLoaderFactoryForBrowserProcess();
  }

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = report.ReportURL();
  resource_request->method = net::HttpRequestHeaders::kPostMethod;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
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
          sender: "Event-level Conversion Measurement API"
          description:
            "The Conversion Measurement API allows sites to measure "
            "conversions (e.g. purchases) and attribute them to clicked ads, "
            "without using cross-site persistent identifiers like third party "
            "cookies."
          trigger:
            "When a registered conversion has become eligible for reporting."
          data:
            "A high-entropy identifier declared by the site in which the user "
            "clicked on an impression. A noisy low-entropy data value declared "
            "on the conversion site."
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

  simple_url_loader_ptr->AttachStringForUpload(
      SerializeAttributionJson(report.ReportBody()), "application/json");

  // Retry once on network change. A network change during DNS resolution
  // results in a DNS error rather than a network change error, so retry in
  // those cases as well.
  // TODO(http://crbug.com/1181106): Consider logging metrics for how often this
  // retry succeeds/fails.
  int retry_mode = network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE |
                   network::SimpleURLLoader::RETRY_ON_NAME_NOT_RESOLVED;
  simple_url_loader_ptr->SetRetryOptions(/*max_retries=*/1, retry_mode);

  // Unretained is safe because the URLLoader is owned by |this| and will be
  // deleted before |this|.
  simple_url_loader_ptr->DownloadHeadersOnly(
      url_loader_factory_.get(),
      base::BindOnce(&AttributionNetworkSenderImpl::OnReportSent,
                     base::Unretained(this), std::move(it), std::move(report),
                     std::move(sent_callback)));
}

void AttributionNetworkSenderImpl::SetURLLoaderFactoryForTesting(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  url_loader_factory_ = url_loader_factory;
}

void AttributionNetworkSenderImpl::OnReportSent(
    UrlLoaderList::iterator it,
    AttributionReport report,
    ReportSentCallback sent_callback,
    scoped_refptr<net::HttpResponseHeaders> headers) {
  network::SimpleURLLoader* loader = it->get();

  // Consider a non-200 HTTP code as a non-internal error.
  int net_error = loader->NetError();
  bool internal_ok =
      net_error == net::OK || net_error == net::ERR_HTTP_RESPONSE_CODE_FAILURE;

  int response_code = headers ? headers->response_code() : -1;
  bool external_ok = response_code == net::HTTP_OK;
  Status status =
      internal_ok && external_ok
          ? Status::kOk
          : !internal_ok ? Status::kInternalError : Status::kExternalError;
  base::UmaHistogramEnumeration("Conversions.ReportStatus", status);

  // Since net errors are always negative and HTTP errors are always positive,
  // it is fine to combine these in a single histogram.
  base::UmaHistogramSparse("Conversions.Report.HttpResponseOrNetErrorCode",
                           internal_ok ? response_code : net_error);

  if (loader->GetNumRetries() > 0) {
    base::UmaHistogramBoolean("Conversions.ReportRetrySucceed",
                              status == Status::kOk);
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
      .Run(std::move(report),
           SendResult(report_status, headers ? headers->response_code() : 0));
}

}  // namespace content
