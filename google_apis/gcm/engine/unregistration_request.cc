// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/engine/unregistration_request.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "google_apis/credentials_mode.h"
#include "google_apis/gcm/base/gcm_util.h"
#include "google_apis/gcm/monitoring/gcm_stats_recorder.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace gcm {

namespace {

const char kRequestContentType[] = "application/x-www-form-urlencoded";

// Request constants.
const char kCategoryKey[] = "app";
const char kSubtypeKey[] = "X-subtype";
const char kDeleteKey[] = "delete";
const char kDeleteValue[] = "true";
const char kDeviceIdKey[] = "device";
const char kLoginHeader[] = "AidLogin";

// Response constants.
const char kErrorPrefix[] = "Error=";
const char kInvalidParameters[] = "INVALID_PARAMETERS";
const char kInternalServerError[] = "InternalServerError";
const char kDeviceRegistrationError[] = "PHONE_REGISTRATION_ERROR";

// Gets correct status from the error message.
UnregistrationRequest::Status GetStatusFromError(const std::string& error) {
  if (base::Contains(error, kInvalidParameters)) {
    return UnregistrationRequest::INVALID_PARAMETERS;
  }
  if (base::Contains(error, kInternalServerError)) {
    return UnregistrationRequest::INTERNAL_SERVER_ERROR;
  }
  if (base::Contains(error, kDeviceRegistrationError)) {
    return UnregistrationRequest::DEVICE_REGISTRATION_ERROR;
  }
  // Should not be reached, unless the server adds new error types.
  return UnregistrationRequest::UNKNOWN_ERROR;
}

// Determines whether to retry based on the status of the last request.
bool ShouldRetryWithStatus(UnregistrationRequest::Status status) {
  switch (status) {
    case UnregistrationRequest::URL_FETCHING_FAILED:
    case UnregistrationRequest::NO_RESPONSE_BODY:
    case UnregistrationRequest::RESPONSE_PARSING_FAILED:
    case UnregistrationRequest::INCORRECT_APP_ID:
    case UnregistrationRequest::SERVICE_UNAVAILABLE:
    case UnregistrationRequest::INTERNAL_SERVER_ERROR:
    case UnregistrationRequest::HTTP_NOT_OK:
      return true;
    case UnregistrationRequest::SUCCESS:
    case UnregistrationRequest::INVALID_PARAMETERS:
    case UnregistrationRequest::DEVICE_REGISTRATION_ERROR:
    case UnregistrationRequest::UNKNOWN_ERROR:
    case UnregistrationRequest::REACHED_MAX_RETRIES:
      return false;
    case UnregistrationRequest::UNREGISTRATION_STATUS_COUNT:
      NOTREACHED();
  }
  return false;
}

}  // namespace

UnregistrationRequest::RequestInfo::RequestInfo(uint64_t android_id,
                                                uint64_t security_token,
                                                const std::string& category,
                                                const std::string& subtype)
    : android_id(android_id),
      security_token(security_token),
      category(category),
      subtype(subtype) {
  DCHECK(android_id != 0UL);
  DCHECK(security_token != 0UL);
  DCHECK(!category.empty());
}

UnregistrationRequest::RequestInfo::~RequestInfo() {}

UnregistrationRequest::CustomRequestHandler::CustomRequestHandler() {}

UnregistrationRequest::CustomRequestHandler::~CustomRequestHandler() {}

UnregistrationRequest::UnregistrationRequest(
    const GURL& registration_url,
    const RequestInfo& request_info,
    std::unique_ptr<CustomRequestHandler> custom_request_handler,
    const net::BackoffEntry::Policy& backoff_policy,
    UnregistrationCallback callback,
    int max_retry_count,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner,
    GCMStatsRecorder* recorder,
    const std::string& source_to_record)
    : callback_(std::move(callback)),
      request_info_(request_info),
      custom_request_handler_(std::move(custom_request_handler)),
      registration_url_(registration_url),
      backoff_entry_(&backoff_policy),
      url_loader_factory_(std::move(url_loader_factory)),
      retries_left_(max_retry_count),
      io_task_runner_(io_task_runner),
      recorder_(recorder),
      source_to_record_(source_to_record) {
  DCHECK(io_task_runner_);
  DCHECK_GE(max_retry_count, 0);
}

UnregistrationRequest::~UnregistrationRequest() {}

void UnregistrationRequest::Start() {
  DCHECK(!callback_.is_null());
  DCHECK(!url_loader_.get());
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("gcm_unregistration", R"(
        semantics {
          sender: "GCM Driver"
          description:
            "Chromium interacts with Google Cloud Messaging to receive push "
            "messages for various browser features, as well as on behalf of "
            "websites and extensions. This requests Google Cloud Messaging to "
            "invalidate the included registration so that it can no longer be "
            "used to distribute messages to Chromium."
          trigger:
            "Immediately after a feature, website or extension removes a "
            "registration they previously created with the GCM Driver."
          data:
            "The profile-bound Android ID and associated secret, and the "
            "identifiers for the feature, website or extension that is "
            "removing the registration."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Support for interacting with Google Cloud Messaging is enabled by "
            "default, and there is no configuration option to completely "
            "disable it."
          policy_exception_justification:
            "Not implemented, considered not useful."
        })");

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = registration_url_;
  request->method = "POST";
  request->credentials_mode =
      google_apis::GetOmitCredentialsModeForGaiaRequests();
  BuildRequestHeaders(&request->headers);

  std::string body;
  BuildRequestBody(&body);

  DVLOG(1) << "Unregistration request: " << body;
  url_loader_ =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation);
  url_loader_->AttachStringForUpload(body, kRequestContentType);
  DVLOG(1) << "Performing unregistration for: " << request_info_.app_id();
  recorder_->RecordUnregistrationSent(request_info_.app_id(),
                                      source_to_record_);
  url_loader_->SetAllowHttpErrorResults(true);
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&UnregistrationRequest::OnURLLoadComplete,
                     base::Unretained(this), url_loader_.get()));
}

void UnregistrationRequest::BuildRequestHeaders(
    net::HttpRequestHeaders* headers) {
  headers->SetHeader(net::HttpRequestHeaders::kAuthorization,
                     std::string(kLoginHeader) + " " +
                         base::NumberToString(request_info_.android_id) + ":" +
                         base::NumberToString(request_info_.security_token));
}

void UnregistrationRequest::BuildRequestBody(std::string* body) {
  BuildFormEncoding(kCategoryKey, request_info_.category, body);
  if (!request_info_.subtype.empty())
    BuildFormEncoding(kSubtypeKey, request_info_.subtype, body);

  BuildFormEncoding(kDeviceIdKey,
                    base::NumberToString(request_info_.android_id), body);
  BuildFormEncoding(kDeleteKey, kDeleteValue, body);

  DCHECK(custom_request_handler_.get());
  custom_request_handler_->BuildRequestBody(body);
}

UnregistrationRequest::Status UnregistrationRequest::ParseResponse(
    const network::SimpleURLLoader* source,
    std::unique_ptr<std::string> body) {
  if (!body) {
    DVLOG(1) << "Unregistration URL fetching failed.";
    return URL_FETCHING_FAILED;
  }

  std::string response = std::move(*body);

  // If we are able to parse a meaningful known error, let's do so. Note that
  // some errors will have HTTP_OK response code!
  if (response.find(kErrorPrefix) != std::string::npos) {
    std::string error = response.substr(response.find(kErrorPrefix) +
                                        std::size(kErrorPrefix) - 1);
    DVLOG(1) << "Unregistration response error message: " << error;
    return GetStatusFromError(error);
  }

  // Can't even get any header info.
  if (!source->ResponseInfo() || !source->ResponseInfo()->headers) {
    DVLOG(1) << "Unregistration HTTP response info or header missing";
    return HTTP_NOT_OK;
  }

  net::HttpStatusCode response_status = static_cast<net::HttpStatusCode>(
      source->ResponseInfo()->headers->response_code());
  if (response_status != net::HTTP_OK) {
    DVLOG(1) << "Unregistration HTTP response code not OK: " << response_status;
    if (response_status == net::HTTP_SERVICE_UNAVAILABLE)
      return SERVICE_UNAVAILABLE;
    if (response_status == net::HTTP_INTERNAL_SERVER_ERROR)
      return INTERNAL_SERVER_ERROR;
    return HTTP_NOT_OK;
  }

  DCHECK(custom_request_handler_.get());
  return custom_request_handler_->ParseResponse(response);
}

void UnregistrationRequest::RetryWithBackoff() {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  DCHECK_GT(retries_left_, 0);
  --retries_left_;
  url_loader_.reset();
  backoff_entry_.InformOfRequest(false);

  DVLOG(1) << "Delaying GCM unregistration of app: " << request_info_.app_id()
           << ", for " << backoff_entry_.GetTimeUntilRelease().InMilliseconds()
           << " milliseconds.";
  recorder_->RecordUnregistrationRetryDelayed(
      request_info_.app_id(), source_to_record_,
      backoff_entry_.GetTimeUntilRelease().InMilliseconds(), retries_left_ + 1);
  DCHECK(!weak_ptr_factory_.HasWeakPtrs());
  io_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&UnregistrationRequest::Start,
                     weak_ptr_factory_.GetWeakPtr()),
      backoff_entry_.GetTimeUntilRelease());
}

void UnregistrationRequest::OnURLLoadComplete(
    const network::SimpleURLLoader* source,
    std::unique_ptr<std::string> body) {
  UnregistrationRequest::Status status = ParseResponse(source, std::move(body));

  DVLOG(1) << "UnregistrationRequestStatus: " << status;

  recorder_->RecordUnregistrationResponse(request_info_.app_id(),
                                          source_to_record_, status);

  if (ShouldRetryWithStatus(status)) {
    if (retries_left_ > 0) {
      RetryWithBackoff();
      return;
    }

    status = REACHED_MAX_RETRIES;
    recorder_->RecordUnregistrationResponse(request_info_.app_id(),
                                            source_to_record_, status);
  }

  std::move(callback_).Run(status);
}

}  // namespace gcm
