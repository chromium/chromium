// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/engine/checkin_request.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "build/chromeos_buildflags.h"
#include "google_apis/credentials_mode.h"
#include "google_apis/gcm/monitoring/gcm_stats_recorder.h"
#include "google_apis/gcm/protocol/checkin.pb.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace gcm {

namespace {
const char kRequestContentType[] = "application/x-protobuf";
const int kRequestVersionValue = 3;
const int kDefaultUserSerialNumber = 0;

// This enum is also used in an UMA histogram (GCMCheckinRequestStatus
// enum defined in tools/metrics/histograms/enums.xml). Hence the entries here
// shouldn't be deleted or re-ordered and new ones should be added to the end,
// and update the GetCheckinRequestStatusString(...) below.
enum class CheckinRequestStatus {
  kSuccess = 0,  // Checkin completed successfully.
  // kUrlFetchingFailed = 1,
  kBadRequest = 2,             // The request was malformed.
  kUnauthorized = 3,           // The security token didn't match the AID.
  kStatusNotOK = 4,            // HTTP status was not OK.
  kResponseParsingFailed = 5,  // Check in response parsing failed.
  kZeroIdOrToken = 6,          // Either returned android id or security token
                               // was zero.
  kFailedNetError = 7,         // A network error was returned.
  kFailedNoResponse = 8,       // No or invalid response info was returned.
  kFailedNoHeaders = 9,        // No or invalid headers were returned.

  // NOTE: always keep this entry at the end. Add new status types only
  // immediately above this line. Make sure to update the corresponding
  // histogram enum accordingly.
  kMaxValue = kFailedNoHeaders,
};

// Returns string representation of enum CheckinRequestStatus.
std::string GetCheckinRequestStatusString(CheckinRequestStatus status) {
  switch (status) {
    case CheckinRequestStatus::kSuccess:
      return "Success";
    case CheckinRequestStatus::kBadRequest:
      return "Failed: HTTP 400 Bad Request";
    case CheckinRequestStatus::kUnauthorized:
      return "Failed: HTTP 401 Unauthorized";
    case CheckinRequestStatus::kStatusNotOK:
      return "Failed: HTTP not OK";
    case CheckinRequestStatus::kResponseParsingFailed:
      return "Failed: Response parsing failed";
    case CheckinRequestStatus::kZeroIdOrToken:
      return "Failed: Zero Android ID or security token";
    case CheckinRequestStatus::kFailedNetError:
      return "Failed: Network error";
    case CheckinRequestStatus::kFailedNoResponse:
      return "Failed: No response";
    case CheckinRequestStatus::kFailedNoHeaders:
      return "Failed: No headers";
  }

  NOTREACHED();
}

// Records checkin status to both stats recorder and reports to UMA.
void RecordCheckinStatusAndReportUMA(CheckinRequestStatus status,
                                     GCMStatsRecorder* recorder,
                                     bool will_retry) {
  base::UmaHistogramEnumeration("GCM.CheckinRequestStatus", status);

  if (status == CheckinRequestStatus::kSuccess)
    recorder->RecordCheckinSuccess();
  else {
    recorder->RecordCheckinFailure(GetCheckinRequestStatusString(status),
                                   will_retry);
  }
}

}  // namespace

CheckinRequest::RequestInfo::RequestInfo(
    uint64_t android_id,
    uint64_t security_token,
    const std::map<std::string, std::string>& account_tokens,
    const std::string& settings_digest,
    const checkin_proto::ChromeBuildProto& chrome_build_proto)
    : android_id(android_id),
      security_token(security_token),
      account_tokens(account_tokens),
      settings_digest(settings_digest),
      chrome_build_proto(chrome_build_proto) {}

CheckinRequest::RequestInfo::RequestInfo(const RequestInfo& other) = default;

CheckinRequest::RequestInfo::~RequestInfo() = default;

CheckinRequest::CheckinRequest(
    const GURL& checkin_url,
    const RequestInfo& request_info,
    const net::BackoffEntry::Policy& backoff_policy,
    CheckinRequestCallback callback,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner,
    GCMStatsRecorder* recorder)
    : url_loader_factory_(url_loader_factory),
      callback_(std::move(callback)),
      backoff_entry_(&backoff_policy),
      checkin_url_(checkin_url),
      request_info_(request_info),
      io_task_runner_(std::move(io_task_runner)),
      recorder_(recorder) {
  DCHECK(io_task_runner_);
}

CheckinRequest::~CheckinRequest() = default;

void CheckinRequest::Start() {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!url_loader_.get());

  checkin_proto::AndroidCheckinRequest request;
  request.set_id(request_info_.android_id);
  request.set_security_token(request_info_.security_token);
  request.set_user_serial_number(kDefaultUserSerialNumber);
  request.set_version(kRequestVersionValue);
  if (!request_info_.settings_digest.empty())
    request.set_digest(request_info_.settings_digest);

  checkin_proto::AndroidCheckinProto* checkin = request.mutable_checkin();
  checkin->mutable_chrome_build()->CopyFrom(request_info_.chrome_build_proto);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  checkin->set_type(checkin_proto::DEVICE_CHROME_OS);
#else
  checkin->set_type(checkin_proto::DEVICE_CHROME_BROWSER);
#endif

  // Pack a map of email -> token mappings into a repeated field, where odd
  // entries are email addresses, while even ones are respective OAuth2 tokens.
  for (std::map<std::string, std::string>::const_iterator iter =
           request_info_.account_tokens.begin();
       iter != request_info_.account_tokens.end(); ++iter) {
    request.add_account_cookie(iter->first);
    request.add_account_cookie(iter->second);
  }

  std::string upload_data;
  CHECK(request.SerializeToString(&upload_data));
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("gcm_checkin", R"(
        semantics {
          sender: "GCM Driver"
          description:
            "Chromium interacts with Google Cloud Messaging to receive push "
            "messages for various browser features, as well as on behalf of "
            "websites and extensions. The check-in periodically verifies the "
            "client's validity with Google servers, and receive updates to "
            "configuration regarding interacting with Google services."
          trigger:
            "Immediately after a feature creates the first Google Cloud "
            "Messaging registration. By default, Chromium will check in with "
            "Google Cloud Messaging every two days. Google can adjust this "
            "interval when it deems necessary."
          data:
            "The profile-bound Android ID and associated secret and account "
            "tokens. A structure containing the Chromium version, channel, and "
            "platform of the host operating system."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Support for interacting with Google Cloud Messaging is enabled by "
            "default, and there is no configuration option to completely "
            "disable it. Websites wishing to receive push messages must "
            "acquire express permission from the user for the 'Notification' "
            "permission."
          policy_exception_justification:
            "Not implemented, considered not useful."
        })");
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = checkin_url_;
  resource_request->method = "POST";
  resource_request->credentials_mode =
      google_apis::GetOmitCredentialsModeForGaiaRequests();

  DVLOG(1) << "Performing check-in request with android id: "
           << request_info_.android_id
           << ", security token: " << request_info_.security_token
           << ", user serial number: " << kDefaultUserSerialNumber
           << ", version: " << kRequestVersionValue
           << "and digest: " << request.digest();
  DVLOG(1) << "Check-in URL: " << checkin_url_.possibly_invalid_spec();
  DVLOG(1) << "Registration request body: " << upload_data;

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);
  url_loader_->AttachStringForUpload(upload_data, kRequestContentType);
  url_loader_->SetAllowHttpErrorResults(true);
  recorder_->RecordCheckinInitiated(request_info_.android_id);
  request_start_time_ = base::TimeTicks::Now();

  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&CheckinRequest::OnURLLoadComplete, base::Unretained(this),
                     url_loader_.get()));
}

void CheckinRequest::RetryWithBackoff() {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());

  backoff_entry_.InformOfRequest(false);
  url_loader_.reset();

  DVLOG(1) << "Delay GCM checkin for: "
           << backoff_entry_.GetTimeUntilRelease().InMilliseconds()
           << " milliseconds.";
  recorder_->RecordCheckinDelayedDueToBackoff(
      backoff_entry_.GetTimeUntilRelease().InMilliseconds());
  DCHECK(!weak_ptr_factory_.HasWeakPtrs());
  io_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CheckinRequest::Start, weak_ptr_factory_.GetWeakPtr()),
      backoff_entry_.GetTimeUntilRelease());
}

void CheckinRequest::OnURLLoadComplete(const network::SimpleURLLoader* source,
                                       std::unique_ptr<std::string> body) {
  if (source->NetError() != net::OK) {
    LOG(ERROR) << "Check-in request got net error: " << source->NetError();
    RecordCheckinStatusAndReportUMA(CheckinRequestStatus::kFailedNetError,
                                    recorder_, /* will_retry= */ true);

    RetryWithBackoff();
    return;
  }

  if (!source->ResponseInfo()) {
    LOG(ERROR) << "Check-in response is missing response info!";
    RecordCheckinStatusAndReportUMA(CheckinRequestStatus::kFailedNoResponse,
                                    recorder_, /* will_retry= */ true);
    RetryWithBackoff();
    return;
  }

  if (!source->ResponseInfo()->headers) {
    LOG(ERROR) << "Check-in response is missing headers!";
    RecordCheckinStatusAndReportUMA(CheckinRequestStatus::kFailedNoHeaders,
                                    recorder_, /* will_retry= */ true);
    RetryWithBackoff();
    return;
  }

  checkin_proto::AndroidCheckinResponse response_proto;

  net::HttpStatusCode response_status = static_cast<net::HttpStatusCode>(
      source->ResponseInfo()->headers->response_code());
  if (response_status == net::HTTP_BAD_REQUEST ||
      response_status == net::HTTP_UNAUTHORIZED) {
    // BAD_REQUEST indicates that the request was malformed.
    // UNAUTHORIZED indicates that security token didn't match the android id.
    CheckinRequestStatus status = response_status == net::HTTP_BAD_REQUEST
                                      ? CheckinRequestStatus::kBadRequest
                                      : CheckinRequestStatus::kUnauthorized;
    LOG(ERROR) << "Check-in response failed with status: "
               << GetCheckinRequestStatusString(status);
    RecordCheckinStatusAndReportUMA(status, recorder_, /* will_retry= */ false);
    std::move(callback_).Run(response_status, response_proto);
    return;
  }

  if (response_status != net::HTTP_OK || !body ||
      !response_proto.ParseFromString(*body)) {
    LOG(ERROR) << "Failed to parse checkin response. HTTP Status: "
               << response_status << ". Retrying.";

    CheckinRequestStatus status =
        response_status != net::HTTP_OK
            ? CheckinRequestStatus::kStatusNotOK
            : CheckinRequestStatus::kResponseParsingFailed;
    RecordCheckinStatusAndReportUMA(status, recorder_, /* will_retry= */ true);
    RetryWithBackoff();
    return;
  }

  if (!response_proto.has_android_id() ||
      !response_proto.has_security_token() ||
      response_proto.android_id() == 0 ||
      response_proto.security_token() == 0) {
    LOG(ERROR) << "Check-in response: "
               << (response_proto.has_android_id()
                       ? (response_proto.android_id() == 0 ? "has 0 AID, "
                                                           : "has valid AID, ")
                       : "is missing AID, ")
               << (response_proto.has_security_token()
                       ? (response_proto.security_token() == 0
                              ? "has 0 security_token"
                              : "has valid security_token")
                       : "is missing security_token");
    RecordCheckinStatusAndReportUMA(CheckinRequestStatus::kZeroIdOrToken,
                                    recorder_, /* will_retry= */ true);
    RetryWithBackoff();
    return;
  }

  DVLOG(1) << "Check-in succeeded. Response has AID: "
           << response_proto.android_id()
           << " and security_token: " << response_proto.security_token();
  RecordCheckinStatusAndReportUMA(CheckinRequestStatus::kSuccess, recorder_,
                                  /* will_retry= */ false);
  std::move(callback_).Run(response_status, response_proto);
}

}  // namespace gcm
