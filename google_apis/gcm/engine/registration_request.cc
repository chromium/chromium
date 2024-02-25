// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/engine/registration_request.h"

#include <stddef.h>

#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram.h"
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
#include "url/gurl.h"

namespace gcm {

namespace {

const char kRegistrationRequestContentType[] =
    "application/x-www-form-urlencoded";

// Request constants.
const char kCategoryKey[] = "app";
const char kSubtypeKey[] = "X-subtype";
const char kDeviceIdKey[] = "device";
const char kLoginHeader[] = "AidLogin";

// Response constants.
const char kErrorPrefix[] = "Error=";
const char kTokenPrefix[] = "token=";
const char kDeviceRegistrationError[] = "PHONE_REGISTRATION_ERROR";
const char kAuthenticationFailed[] = "AUTHENTICATION_FAILED";
const char kInvalidSender[] = "INVALID_SENDER";
const char kInvalidParameters[] = "INVALID_PARAMETERS";
const char kInternalServerError[] = "INTERNAL_SERVER_ERROR";
const char kQuotaExceeded[] = "QUOTA_EXCEEDED";
const char kTooManyRegistrations[] = "TOO_MANY_REGISTRATIONS";
const char kTooManySubscribers[] = "TOO_MANY_SUBSCRIBERS";
const char kInvalidTargetVersion[] = "INVALID_TARGET_VERSION";
const char kFisAuthError[] = "FIS_AUTH_ERROR";

// Gets correct status from the error message.
RegistrationRequest::Status GetStatusFromError(const std::string& error) {
  if (base::Contains(error, kDeviceRegistrationError)) {
    return RegistrationRequest::DEVICE_REGISTRATION_ERROR;
  }
  if (base::Contains(error, kAuthenticationFailed)) {
    return RegistrationRequest::AUTHENTICATION_FAILED;
  }
  if (base::Contains(error, kInvalidSender)) {
    return RegistrationRequest::INVALID_SENDER;
  }
  if (base::Contains(error, kInvalidParameters)) {
    return RegistrationRequest::INVALID_PARAMETERS;
  }
  if (base::Contains(error, kInternalServerError)) {
    return RegistrationRequest::INTERNAL_SERVER_ERROR;
  }
  if (base::Contains(error, kQuotaExceeded)) {
    return RegistrationRequest::QUOTA_EXCEEDED;
  }
  if (base::Contains(error, kTooManyRegistrations)) {
    return RegistrationRequest::TOO_MANY_REGISTRATIONS;
  }
  if (base::Contains(error, kTooManySubscribers)) {
    return RegistrationRequest::TOO_MANY_SUBSCRIBERS;
  }
  if (base::Contains(error, kInvalidTargetVersion)) {
    return RegistrationRequest::INVALID_TARGET_VERSION;
  }
  if (base::Contains(error, kFisAuthError)) {
    return RegistrationRequest::FIS_AUTH_ERROR;
  }
  // Should not be reached, unless the server adds new error types.
  return RegistrationRequest::UNKNOWN_ERROR;
}

// Determines whether to retry based on the status of the last request.
bool ShouldRetryWithStatus(RegistrationRequest::Status status) {
  switch (status) {
    case RegistrationRequest::AUTHENTICATION_FAILED:
    case RegistrationRequest::DEVICE_REGISTRATION_ERROR:
    case RegistrationRequest::UNKNOWN_ERROR:
    case RegistrationRequest::URL_FETCHING_FAILED:
    case RegistrationRequest::HTTP_NOT_OK:
    case RegistrationRequest::NO_RESPONSE_BODY:
    case RegistrationRequest::RESPONSE_PARSING_FAILED:
    case RegistrationRequest::INTERNAL_SERVER_ERROR:
    case RegistrationRequest::TOO_MANY_SUBSCRIBERS:
    case RegistrationRequest::FIS_AUTH_ERROR:
      return true;
    case RegistrationRequest::SUCCESS:
    case RegistrationRequest::INVALID_PARAMETERS:
    case RegistrationRequest::INVALID_SENDER:
    case RegistrationRequest::QUOTA_EXCEEDED:
    case RegistrationRequest::TOO_MANY_REGISTRATIONS:
    case RegistrationRequest::REACHED_MAX_RETRIES:
    case RegistrationRequest::INVALID_TARGET_VERSION:
      return false;
  }
  return false;
}

}  // namespace

RegistrationRequest::RequestInfo::RequestInfo(uint64_t android_id,
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

RegistrationRequest::RequestInfo::~RequestInfo() {}

RegistrationRequest::CustomRequestHandler::CustomRequestHandler() {}

RegistrationRequest::CustomRequestHandler::~CustomRequestHandler() {}

RegistrationRequest::RegistrationRequest(
    const GURL& registration_url,
    const RequestInfo& request_info,
    std::unique_ptr<CustomRequestHandler> custom_request_handler,
    const net::BackoffEntry::Policy& backoff_policy,
    RegistrationCallback callback,
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

RegistrationRequest::~RegistrationRequest() {}

void RegistrationRequest::Start() {
  DCHECK(!callback_.is_null());
  DCHECK(!url_loader_.get());
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("gcm_registration", R"(
        semantics {
          sender: "GCM Driver"
          description:
            "Chromium interacts with Google Cloud Messaging to receive push "
            "messages for various browser features, as well as on behalf of "
            "websites and extensions. This requests Google Cloud Messaging to "
            "create a new subscription through which messages can be sent to "
            "the registering entity, through Chromium."
          trigger:
            "Immediately after a feature, website or extension creates a new "
            "registration with the GCM Driver. Repeated registration requests "
            "will be served from the cache instead."
          data:
            "The profile-bound Android ID and associated secret, and the "
            "identifiers for the feature, website or extension that is "
            "creating the registration."
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

  DVLOG(1) << "Performing registration for: " << request_info_.app_id()
           << ", with android id: " << request_info_.android_id
           << " and security token: " << request_info_.security_token;
  DVLOG(1) << "Registration URL: " << registration_url_.possibly_invalid_spec();
  DVLOG(1) << "Registration request headers: " << request->headers.ToString();
  DVLOG(1) << "Registration request body: " << body;

  url_loader_ =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation);
  url_loader_->AttachStringForUpload(body, kRegistrationRequestContentType);
  recorder_->RecordRegistrationSent(request_info_.app_id(), source_to_record_);
  url_loader_->SetAllowHttpErrorResults(true);
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&RegistrationRequest::OnURLLoadComplete,
                     base::Unretained(this), url_loader_.get()));
}

void RegistrationRequest::BuildRequestHeaders(
    net::HttpRequestHeaders* headers) {
  headers->SetHeader(net::HttpRequestHeaders::kAuthorization,
                     std::string(kLoginHeader) + " " +
                         base::NumberToString(request_info_.android_id) + ":" +
                         base::NumberToString(request_info_.security_token));
}

void RegistrationRequest::BuildRequestBody(std::string* body) {
  BuildFormEncoding(kCategoryKey, request_info_.category, body);
  if (!request_info_.subtype.empty())
    BuildFormEncoding(kSubtypeKey, request_info_.subtype, body);

  BuildFormEncoding(kDeviceIdKey,
                    base::NumberToString(request_info_.android_id), body);

  DCHECK(custom_request_handler_.get());
  custom_request_handler_->BuildRequestBody(body);
}

void RegistrationRequest::RetryWithBackoff() {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  DCHECK_GT(retries_left_, 0);
  --retries_left_;
  url_loader_.reset();
  backoff_entry_.InformOfRequest(false);

  VLOG(1) << "Delaying GCM registration of app: " << request_info_.app_id()
          << ", for " << backoff_entry_.GetTimeUntilRelease().InMilliseconds()
          << " milliseconds.";
  recorder_->RecordRegistrationRetryDelayed(
      request_info_.app_id(), source_to_record_,
      backoff_entry_.GetTimeUntilRelease().InMilliseconds(), retries_left_ + 1);
  DCHECK(!weak_ptr_factory_.HasWeakPtrs());
  io_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&RegistrationRequest::Start,
                     weak_ptr_factory_.GetWeakPtr()),
      backoff_entry_.GetTimeUntilRelease());
}

RegistrationRequest::Status RegistrationRequest::ParseResponse(
    const network::SimpleURLLoader* source,
    std::unique_ptr<std::string> body,
    std::string* token) {
  if (source->NetError() != net::OK) {
    LOG(ERROR) << "Registration URL fetching failed.";
    return URL_FETCHING_FAILED;
  }

  std::string response;
  if (!body) {
    LOG(ERROR) << "Failed to get registration response body.";
    return NO_RESPONSE_BODY;
  }
  response = std::move(*body);

  // If we are able to parse a meaningful known error, let's do so. Note that
  // some errors will have HTTP_OK response code!
  size_t error_pos = response.find(kErrorPrefix);
  if (error_pos != std::string::npos) {
    std::string error =
        response.substr(error_pos + std::size(kErrorPrefix) - 1);
    LOG(ERROR) << "Registration response error message: " << error;
    RegistrationRequest::Status status = GetStatusFromError(error);
    return status;
  }

  // Can't even get any header info.
  if (!source->ResponseInfo() || !source->ResponseInfo()->headers) {
    LOG(ERROR) << "Registration HTTP response info or header missing";
    return HTTP_NOT_OK;
  }

  // If we cannot tell what the error is, but at least we know response code was
  // not OK.
  if (source->ResponseInfo()->headers->response_code() != net::HTTP_OK) {
    LOG(ERROR) << "Registration HTTP response code not OK: "
               << source->ResponseInfo()->headers->response_code();
    return HTTP_NOT_OK;
  }

  size_t token_pos = response.find(kTokenPrefix);
  if (token_pos != std::string::npos) {
    *token = response.substr(token_pos + std::size(kTokenPrefix) - 1);
    return SUCCESS;
  }

  LOG(ERROR) << "Registration HTTP response parsing failed";
  return RESPONSE_PARSING_FAILED;
}

void RegistrationRequest::OnURLLoadComplete(
    const network::SimpleURLLoader* source,
    std::unique_ptr<std::string> body) {
  std::string token;
  Status status = ParseResponse(source, std::move(body), &token);
  recorder_->RecordRegistrationResponse(request_info_.app_id(),
                                        source_to_record_, status);

  DCHECK(custom_request_handler_.get());
  custom_request_handler_->ReportStatusToUMA(status, request_info_.subtype);
  custom_request_handler_->ReportNetErrorCodeToUMA(source->NetError());

  if (ShouldRetryWithStatus(status)) {
    if (retries_left_ > 0) {
      RetryWithBackoff();
      return;
    }

    status = REACHED_MAX_RETRIES;
    recorder_->RecordRegistrationResponse(request_info_.app_id(),
                                          source_to_record_, status);

    DCHECK(custom_request_handler_.get());
    custom_request_handler_->ReportStatusToUMA(status, request_info_.subtype);
  }

  std::move(callback_).Run(status, token);
}

}  // namespace gcm
