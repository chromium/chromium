// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/engine/checkin_request.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "google_apis/gcm/monitoring/gcm_stats_recorder.h"
#include "google_apis/gcm/protocol/checkin.pb.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace gcm {

namespace {
const char kRequestContentType[] = "application/x-protobuf";
const int kRequestVersionValue = 3;
const int kDefaultUserSerialNumber = 0;

// This enum is also used in an UMA histogram (GCMCheckinRequestStatus
// enum defined in tools/metrics/histograms/histogram.xml). Hence the entries
// here shouldn't be deleted or re-ordered and new ones should be added to
// the end, and update the GetCheckinRequestStatusString(...) below.
enum CheckinRequestStatus {
  SUCCESS,                    // Checkin completed successfully.
  URL_FETCHING_FAILED,        // URL fetching failed.
  HTTP_BAD_REQUEST,           // The request was malformed.
  HTTP_UNAUTHORIZED,          // The security token didn't match the android id.
  HTTP_NOT_OK,                // HTTP status was not OK.
  RESPONSE_PARSING_FAILED,    // Check in response parsing failed.
  ZERO_ID_OR_TOKEN,           // Either returned android id or security token
                              // was zero.
  // NOTE: always keep this entry at the end. Add new status types only
  // immediately above this line. Make sure to update the corresponding
  // histogram enum accordingly.
  STATUS_COUNT
};

// Returns string representation of enum CheckinRequestStatus.
std::string GetCheckinRequestStatusString(CheckinRequestStatus status) {
  switch (status) {
    case SUCCESS:
      return "SUCCESS";
    case URL_FETCHING_FAILED:
      return "URL_FETCHING_FAILED";
    case HTTP_BAD_REQUEST:
      return "HTTP_BAD_REQUEST";
    case HTTP_UNAUTHORIZED:
      return "HTTP_UNAUTHORIZED";
    case HTTP_NOT_OK:
      return "HTTP_NOT_OK";
    case RESPONSE_PARSING_FAILED:
      return "RESPONSE_PARSING_FAILED";
    case ZERO_ID_OR_TOKEN:
      return "ZERO_ID_OR_TOKEN";
    case STATUS_COUNT:
      NOTREACHED();
      break;
  }
  return "UNKNOWN_STATUS";
}

// Records checkin status to both stats recorder and reports to UMA.
void RecordCheckinStatusAndReportUMA(CheckinRequestStatus status,
                                     GCMStatsRecorder* recorder,
                                     bool will_retry) {
  UMA_HISTOGRAM_ENUMERATION("GCM.CheckinRequestStatus", status, STATUS_COUNT);
  if (status == SUCCESS)
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

CheckinRequest::RequestInfo::~RequestInfo() {}

CheckinRequest::CheckinRequest(
    const GURL& checkin_url,
    const RequestInfo& request_info,
    const net::BackoffEntry::Policy& backoff_policy,
    const CheckinRequestCallback& callback,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner,
    GCMStatsRecorder* recorder)
    : url_loader_factory_(url_loader_factory),
      callback_(callback),
      backoff_entry_(&backoff_policy),
      checkin_url_(checkin_url),
      request_info_(request_info),
      io_task_runner_(std::move(io_task_runner)),
      recorder_(recorder) {
  DCHECK(io_task_runner_);
}

CheckinRequest::~CheckinRequest() {}

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
#if defined(CHROME_OS)
  checkin->set_type(checkin_proto::DEVICE_CHROME_OS);
#else
  checkin->set_type(checkin_proto::DEVICE_CHROME_BROWSER);
#endif

  // Pack a map of email -> token mappings into a repeated field, where odd
  // entries are email addresses, while even ones are respective OAuth2 tokens.
  for (std::map<std::string, std::string>::const_iterator iter =
           request_info_.account_tokens.begin();
       iter != request_info_.account_tokens.end();
       ++iter) {
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
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
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
  checkin_proto::AndroidCheckinResponse response_proto;
  if (source->NetError() != net::OK || !source->ResponseInfo() ||
      !source->ResponseInfo()->headers) {
    LOG(ERROR) << "Failed to get checkin response. Fetcher failed. Retrying.";
    RecordCheckinStatusAndReportUMA(URL_FETCHING_FAILED, recorder_, true);
    RetryWithBackoff();
    return;
  }

  net::HttpStatusCode response_status = static_cast<net::HttpStatusCode>(
      source->ResponseInfo()->headers->response_code());
  if (response_status == net::HTTP_BAD_REQUEST ||
      response_status == net::HTTP_UNAUTHORIZED) {
    // BAD_REQUEST indicates that the request was malformed.
    // UNAUTHORIZED indicates that security token didn't match the android id.
    LOG(ERROR) << "No point retrying the checkin with status: "
               << response_status << ". Checkin failed.";
    CheckinRequestStatus status = response_status == net::HTTP_BAD_REQUEST ?
        HTTP_BAD_REQUEST : HTTP_UNAUTHORIZED;
    RecordCheckinStatusAndReportUMA(status, recorder_, false);
    callback_.Run(response_status, response_proto);
    return;
  }

  if (response_status != net::HTTP_OK || !body ||
      !response_proto.ParseFromString(*body)) {
    LOG(ERROR) << "Failed to get checkin response. HTTP Status: "
               << response_status << ". Retrying.";
    CheckinRequestStatus status = response_status != net::HTTP_OK ?
        HTTP_NOT_OK : RESPONSE_PARSING_FAILED;
    RecordCheckinStatusAndReportUMA(status, recorder_, true);
    RetryWithBackoff();
    return;
  }

  if (!response_proto.has_android_id() ||
      !response_proto.has_security_token() ||
      response_proto.android_id() == 0 ||
      response_proto.security_token() == 0) {
    LOG(ERROR) << "Android ID or security token is 0. Retrying.";
    RecordCheckinStatusAndReportUMA(ZERO_ID_OR_TOKEN, recorder_, true);
    RetryWithBackoff();
    return;
  }

  RecordCheckinStatusAndReportUMA(SUCCESS, recorder_, false);
  callback_.Run(response_status, response_proto);
}

}  // namespace gcm
