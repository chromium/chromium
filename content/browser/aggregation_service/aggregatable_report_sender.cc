// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/aggregatable_report_sender.h"

#include <memory>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/check.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/isolation_info.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace content {

AggregatableReportSender::AggregatableReportSender(
    StoragePartition* storage_partition)
    : storage_partition_(storage_partition) {
  DCHECK(storage_partition_);
}

AggregatableReportSender::AggregatableReportSender(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    bool enable_debug_logging)
    : url_loader_factory_(std::move(url_loader_factory)),
      enable_debug_logging_(enable_debug_logging) {
  DCHECK(url_loader_factory_);
}

AggregatableReportSender::~AggregatableReportSender() = default;

// static
std::unique_ptr<AggregatableReportSender>
AggregatableReportSender::CreateForTesting(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    bool enable_debug_logging) {
  return base::WrapUnique(new AggregatableReportSender(
      std::move(url_loader_factory), enable_debug_logging));
}

void AggregatableReportSender::SendReport(const GURL& url,
                                          const base::Value& contents,
                                          ReportSentCallback callback) {
  DCHECK(storage_partition_ || url_loader_factory_);

  // The browser process URLLoaderFactory is not created by default, so don't
  // create it until it is directly needed.
  if (!url_loader_factory_) {
    url_loader_factory_ =
        storage_partition_->GetURLLoaderFactoryForBrowserProcess();
  }

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  resource_request->method = net::HttpRequestHeaders::kPostMethod;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->load_flags =
      net::LOAD_DISABLE_CACHE | net::LOAD_BYPASS_CACHE;
  resource_request->trusted_params = network::ResourceRequest::TrustedParams();
  resource_request->trusted_params->isolation_info =
      net::IsolationInfo::CreateTransient();

  // TODO(crbug.com/1238343): Update the "policy" field in the traffic
  // annotation when a setting to disable the API is properly
  // surfaced/implemented.
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("aggregation_service_report", R"(
        semantics {
          sender: "Aggregation Service"
          description:
            "Sends the aggregatable report to reporting endpoint requested by "
            "APIs that rely on private, secure aggregation (e.g. Attribution "
            "Reporting API, see "
            "https://github.com/WICG/attribution-reporting-api)."
          trigger:
            "When an aggregatable report has become eligible for reporting."
          data:
            "The aggregatable report encoded in JSON format."
          destination: OTHER
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be disabled by settings."
          policy_exception_justification:
            "Not implemented yet. The feature is used by a command line tool, "
            "but not yet integrated with the browser."
        })");

  auto simple_url_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  network::SimpleURLLoader* simple_url_loader_ptr = simple_url_loader.get();

  auto it = loaders_in_progress_.insert(loaders_in_progress_.begin(),
                                        std::move(simple_url_loader));
  simple_url_loader_ptr->SetTimeoutDuration(base::Seconds(30));

  std::string contents_json;

  // TODO(crbug.com/1244991): Check for required fields of contents.
  bool succeeded = base::JSONWriter::Write(contents, &contents_json);
  DCHECK(succeeded);
  simple_url_loader_ptr->AttachStringForUpload(contents_json,
                                               "application/json");

  const int kMaxRetries = 1;

  // Retry on a network change. A network change during DNS resolution
  // results in a DNS error rather than a network change error, so retry in
  // those cases as well.
  int retry_mode = network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE |
                   network::SimpleURLLoader::RETRY_ON_NAME_NOT_RESOLVED;
  simple_url_loader_ptr->SetRetryOptions(kMaxRetries, retry_mode);

  // Allow bodies of non-2xx responses to be returned.
  simple_url_loader_ptr->SetAllowHttpErrorResults(true);

  // Unretained is safe because the URLLoader is owned by `this` and will be
  // deleted before `this`.
  simple_url_loader_ptr->DownloadHeadersOnly(
      url_loader_factory_.get(),
      base::BindOnce(&AggregatableReportSender::OnReportSent,
                     base::Unretained(this), std::move(it),
                     std::move(callback)));
}

void AggregatableReportSender::OnReportSent(
    UrlLoaderList::iterator it,
    ReportSentCallback callback,
    scoped_refptr<net::HttpResponseHeaders> headers) {
  RequestStatus status;

  absl::optional<int> http_response_code;
  if (headers)
    http_response_code = headers->response_code();

  network::SimpleURLLoader* loader = it->get();
  if (loader->NetError() != net::OK) {
    status = RequestStatus::kNetworkError;
  } else if (http_response_code == net::HTTP_OK) {
    status = RequestStatus::kOk;
  } else {
    status = RequestStatus::kServerError;
  }

  if (enable_debug_logging_ && status != RequestStatus::kOk) {
    LOG(ERROR) << "Report sending failed, net error: "
               << net::ErrorToShortString(loader->NetError())
               << ", HTTP response code: "
               << (http_response_code
                       ? base::NumberToString(*http_response_code)
                       : "N/A");
  }

  base::UmaHistogramEnumeration(
      "PrivacySandbox.AggregationService.ReportSender.Status", status);

  // Since net errors are always negative and HTTP errors are always positive,
  // it is fine to combine these in a single histogram.
  base::UmaHistogramSparse(
      "PrivacySandbox.AggregationService.ReportSender."
      "HttpResponseOrNetErrorCode",
      loader->NetError() != net::OK ? loader->NetError()
                                    : http_response_code.value_or(1));

  loaders_in_progress_.erase(it);
  std::move(callback).Run(status);
}

}  // namespace content
