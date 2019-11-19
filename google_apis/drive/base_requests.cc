// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/drive/base_requests.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "google_apis/drive/drive_api_parser.h"
#include "google_apis/drive/request_sender.h"
#include "google_apis/drive/request_util.h"
#include "google_apis/drive/task_util.h"
#include "google_apis/drive/time_util.h"
#include "net/base/load_flags.h"
#include "net/base/mime_util.h"
#include "net/http/http_util.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace {

// Template for optional OAuth2 authorization HTTP header.
const char kAuthorizationHeaderFormat[] = "Authorization: Bearer %s";
// Template for GData API version HTTP header.
const char kGDataVersionHeader[] = "GData-Version: 3.0";

// Maximum number of attempts for re-authentication per request.
const int kMaxReAuthenticateAttemptsPerRequest = 1;

// Template for initiate upload of both GData WAPI and Drive API v2.
const char kUploadContentType[] = "X-Upload-Content-Type: ";
const char kUploadContentLength[] = "X-Upload-Content-Length: ";
const char kUploadResponseLocation[] = "location";

// Template for upload data range of both GData WAPI and Drive API v2.
const char kUploadContentRange[] = "Content-Range: bytes ";
const char kUploadResponseRange[] = "range";

// Mime type of JSON.
const char kJsonMimeType[] = "application/json";

// Mime type of multipart related.
const char kMultipartRelatedMimeTypePrefix[] =
    "multipart/related; boundary=";

// Mime type of multipart mixed.
const char kMultipartMixedMimeTypePrefix[] =
    "multipart/mixed; boundary=";

// Header for each item in a multipart message.
const char kMultipartItemHeaderFormat[] = "--%s\nContent-Type: %s\n\n";

// Footer for whole multipart message.
const char kMultipartFooterFormat[] = "--%s--";

// Parses JSON passed in |json| on |blocking_task_runner|. Runs |callback| on
// the calling thread when finished with either success or failure.
// The callback must not be null.
void ParseJsonOnBlockingPool(
    base::TaskRunner* blocking_task_runner,
    std::string json,
    base::OnceCallback<void(std::unique_ptr<base::Value> value)> callback) {
  base::PostTaskAndReplyWithResult(
      blocking_task_runner, FROM_HERE,
      base::BindOnce(&google_apis::ParseJson, std::move(json)),
      std::move(callback));
}

// Returns response headers as a string. Returns a warning message if
// |response_head| does not contain a valid response. Used only for debugging.
std::string GetResponseHeadersAsString(
    const network::mojom::URLResponseHead& response_head) {
  // Check that response code indicates response headers are valid (i.e. not
  // malformed) before we retrieve the headers.
  if (response_head.headers->response_code() == -1)
    return "Response headers are malformed!!";

  return response_head.headers->raw_headers();
}

// Obtains the multipart body for the metadata string and file contents. If
// predetermined_boundary is empty, the function generates the boundary string.
bool GetMultipartContent(const std::string& predetermined_boundary,
                         const std::string& metadata_json,
                         const std::string& content_type,
                         const base::FilePath& path,
                         std::string* upload_content_type,
                         std::string* upload_content_data) {
  std::vector<google_apis::ContentTypeAndData> parts(2);
  parts[0].type = kJsonMimeType;
  parts[0].data = metadata_json;
  parts[1].type = content_type;
  if (!ReadFileToString(path, &parts[1].data))
    return false;

  google_apis::ContentTypeAndData output;
  GenerateMultipartBody(google_apis::MULTIPART_RELATED, predetermined_boundary,
                        parts, &output, nullptr);
  upload_content_type->swap(output.type);
  upload_content_data->swap(output.data);
  return true;
}

// Parses JSON body and returns corresponding DriveApiErrorCode if it is found.
// The server may return detailed error status in JSON.
// See https://developers.google.com/drive/handle-errors
google_apis::DriveApiErrorCode MapJsonError(
    google_apis::DriveApiErrorCode code,
    const std::string& error_body) {
  if (IsSuccessfulDriveApiErrorCode(code))
    return code;

  DVLOG(1) << error_body;
  const char kErrorKey[] = "error";
  const char kErrorErrorsKey[] = "errors";
  const char kErrorReasonKey[] = "reason";
  const char kErrorMessageKey[] = "message";
  const char kErrorReasonRateLimitExceeded[] = "rateLimitExceeded";
  const char kErrorReasonUserRateLimitExceeded[] = "userRateLimitExceeded";
  const char kErrorReasonQuotaExceeded[] = "quotaExceeded";
  const char kErrorReasonResponseTooLarge[] = "responseTooLarge";

  std::unique_ptr<const base::Value> value(google_apis::ParseJson(error_body));
  const base::DictionaryValue* dictionary = nullptr;
  const base::DictionaryValue* error = nullptr;
  if (value &&
      value->GetAsDictionary(&dictionary) &&
      dictionary->GetDictionaryWithoutPathExpansion(kErrorKey, &error)) {
    // Get error message.
    std::string message;
    error->GetStringWithoutPathExpansion(kErrorMessageKey, &message);
    DLOG(ERROR) << "code: " << code << ", message: " << message;

    // Override the error code based on the reason of the first error.
    const base::ListValue* errors = nullptr;
    const base::DictionaryValue* first_error = nullptr;
    if (error->GetListWithoutPathExpansion(kErrorErrorsKey, &errors) &&
        errors->GetDictionary(0, &first_error)) {
      std::string reason;
      first_error->GetStringWithoutPathExpansion(kErrorReasonKey, &reason);
      if (reason == kErrorReasonRateLimitExceeded ||
          reason == kErrorReasonUserRateLimitExceeded) {
        return google_apis::HTTP_SERVICE_UNAVAILABLE;
      }
      if (reason == kErrorReasonQuotaExceeded)
        return google_apis::DRIVE_NO_SPACE;
      if (reason == kErrorReasonResponseTooLarge)
        return google_apis::DRIVE_RESPONSE_TOO_LARGE;
    }
  }

  return code;
}

// Used to release (and close) a file on a blocking task.
void CloseFile(base::File file) {}

}  // namespace

namespace google_apis {

std::unique_ptr<base::Value> ParseJson(const std::string& json) {
  int error_code = -1;
  std::string error_message;
  std::unique_ptr<base::Value> value =
      base::JSONReader::ReadAndReturnErrorDeprecated(
          json, base::JSON_PARSE_RFC, &error_code, &error_message);

  if (!value.get()) {
    std::string trimmed_json;
    if (json.size() < 80) {
      trimmed_json  = json;
    } else {
      // Take the first 50 and the last 10 bytes.
      trimmed_json =
          base::StringPrintf("%s [%s bytes] %s", json.substr(0, 50).c_str(),
                             base::NumberToString(json.size() - 60).c_str(),
                             json.substr(json.size() - 10).c_str());
    }
    LOG(WARNING) << "Error while parsing entry response: " << error_message
                 << ", code: " << error_code << ", json:\n" << trimmed_json;
  }
  return value;
}

void GenerateMultipartBody(MultipartType multipart_type,
                           const std::string& predetermined_boundary,
                           const std::vector<ContentTypeAndData>& parts,
                           ContentTypeAndData* output,
                           std::vector<uint64_t>* data_offset) {
  std::string boundary;
  // Generate random boundary.
  if (predetermined_boundary.empty()) {
    while (true) {
      boundary = net::GenerateMimeMultipartBoundary();
      bool conflict_with_content = false;
      for (auto& part : parts) {
        if (part.data.find(boundary, 0) != std::string::npos) {
          conflict_with_content = true;
          break;
        }
      }
      if (!conflict_with_content)
        break;
    }
  } else {
    boundary = predetermined_boundary;
  }

  switch (multipart_type) {
    case MULTIPART_RELATED:
      output->type = kMultipartRelatedMimeTypePrefix + boundary;
      break;
    case MULTIPART_MIXED:
      output->type = kMultipartMixedMimeTypePrefix + boundary;
      break;
  }

  output->data.clear();
  if (data_offset)
    data_offset->clear();
  for (auto& part : parts) {
    output->data.append(base::StringPrintf(
        kMultipartItemHeaderFormat, boundary.c_str(), part.type.c_str()));
    if (data_offset)
      data_offset->push_back(output->data.size());
    output->data.append(part.data);
    output->data.append("\n");
  }
  output->data.append(
      base::StringPrintf(kMultipartFooterFormat, boundary.c_str()));
}

//============================ UrlFetchRequestBase ===========================

UrlFetchRequestBase::UrlFetchRequestBase(
    RequestSender* sender,
    const ProgressCallback& upload_progress_callback,
    const ProgressCallback& download_progress_callback)
    : re_authenticate_count_(0),
      sender_(sender),
      upload_progress_callback_(upload_progress_callback),
      download_progress_callback_(download_progress_callback),
      response_content_length_(-1) {}

UrlFetchRequestBase::~UrlFetchRequestBase() {}

void UrlFetchRequestBase::Start(const std::string& access_token,
                                const std::string& custom_user_agent,
                                const ReAuthenticateCallback& callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(!access_token.empty());
  DCHECK(!callback.is_null());
  DCHECK(re_authenticate_callback_.is_null());
  Prepare(base::Bind(&UrlFetchRequestBase::StartAfterPrepare,
                     weak_ptr_factory_.GetWeakPtr(), access_token,
                     custom_user_agent, callback));
}

void UrlFetchRequestBase::Prepare(const PrepareCallback& callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(!callback.is_null());
  callback.Run(HTTP_SUCCESS);
}

void UrlFetchRequestBase::StartAfterPrepare(
    const std::string& access_token,
    const std::string& custom_user_agent,
    const ReAuthenticateCallback& callback,
    DriveApiErrorCode code) {
  DCHECK(CalledOnValidThread());
  DCHECK(!access_token.empty());
  DCHECK(!callback.is_null());
  DCHECK(re_authenticate_callback_.is_null());

  const GURL url = GetURL();
  DriveApiErrorCode error_code;
  if (IsSuccessfulDriveApiErrorCode(code))
    error_code = code;
  else if (url.is_empty())
    error_code = DRIVE_OTHER_ERROR;
  else
    error_code = HTTP_SUCCESS;

  if (error_code != HTTP_SUCCESS) {
    // Error is found on generating the url or preparing the request. Send the
    // error message to the callback, and then return immediately without trying
    // to connect to the server.  We need to call CompleteRequestWithError
    // asynchronously because client code does not assume result callback is
    // called synchronously.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&UrlFetchRequestBase::CompleteRequestWithError,
                       weak_ptr_factory_.GetWeakPtr(), error_code));
    return;
  }

  re_authenticate_callback_ = callback;
  DVLOG(1) << "URL: " << url.spec();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = url;
  request->method = GetRequestType();
  request->load_flags = net::LOAD_DISABLE_CACHE;
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  // Add request headers.
  // Note that SetHeader clears the current headers and sets it to the passed-in
  // headers, so calling it for each header will result in only the last header
  // being set in request headers.
  if (!custom_user_agent.empty())
    request->headers.SetHeader("User-Agent", custom_user_agent);
  request->headers.AddHeaderFromString(kGDataVersionHeader);
  request->headers.AddHeaderFromString(
      base::StringPrintf(kAuthorizationHeaderFormat, access_token.data()));
  for (const auto& header : GetExtraRequestHeaders()) {
    request->headers.AddHeaderFromString(header);
    DVLOG(1) << "Extra header: " << header;
  }

  url_loader_ = network::SimpleURLLoader::Create(
      std::move(request), sender_->get_traffic_annotation_tag());
  url_loader_->SetAllowHttpErrorResults(true /* allow */);

  download_data_ = std::make_unique<DownloadData>(blocking_task_runner());
  GetOutputFilePath(&download_data_->output_file_path,
                    &download_data_->get_content_callback);
  if (!download_data_->get_content_callback.is_null()) {
    download_data_->get_content_callback =
        CreateRelayCallback(download_data_->get_content_callback);
  }

  // Set upload data if available.
  std::string upload_content_type;
  std::string upload_content;
  if (GetContentData(&upload_content_type, &upload_content)) {
    url_loader_->AttachStringForUpload(upload_content, upload_content_type);
  } else {
    base::FilePath local_file_path;
    int64_t range_offset = 0;
    int64_t range_length = 0;
    if (GetContentFile(&local_file_path, &range_offset, &range_length,
                       &upload_content_type)) {
      url_loader_->AttachFileForUpload(local_file_path, upload_content_type,
                                       range_offset, range_length);
    }
  }

  if (!upload_progress_callback_.is_null()) {
    url_loader_->SetOnUploadProgressCallback(base::BindRepeating(
        &UrlFetchRequestBase::OnUploadProgress, weak_ptr_factory_.GetWeakPtr(),
        upload_progress_callback_));
  }
  if (!download_progress_callback_.is_null()) {
    url_loader_->SetOnDownloadProgressCallback(base::BindRepeating(
        &UrlFetchRequestBase::OnDownloadProgress,
        weak_ptr_factory_.GetWeakPtr(), download_progress_callback_));
  }

  url_loader_->SetOnResponseStartedCallback(base::BindOnce(
      &UrlFetchRequestBase::OnResponseStarted, weak_ptr_factory_.GetWeakPtr()));

  url_loader_->DownloadAsStream(sender_->url_loader_factory(), this);
}

void UrlFetchRequestBase::OnDownloadProgress(
    const ProgressCallback& progress_callback,
    uint64_t current) {
  progress_callback.Run(static_cast<int64_t>(current),
                        response_content_length_);
}

void UrlFetchRequestBase::OnUploadProgress(
    const ProgressCallback& progress_callback,
    uint64_t position,
    uint64_t total) {
  progress_callback.Run(static_cast<int64_t>(position),
                        static_cast<int64_t>(total));
}

void UrlFetchRequestBase::OnResponseStarted(
    const GURL& final_url,
    const network::mojom::URLResponseHead& response_head) {
  DVLOG(1) << "Response headers:\n"
           << GetResponseHeadersAsString(response_head);
  response_content_length_ = response_head.content_length;
}

// static
bool UrlFetchRequestBase::WriteFileData(std::string file_data,
                                        DownloadData* download_data) {
  if (!download_data->output_file.IsValid()) {
    download_data->output_file.Initialize(
        download_data->output_file_path,
        base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
    if (!download_data->output_file.IsValid())
      return false;
  }
  if (download_data->output_file.WriteAtCurrentPos(file_data.data(),
                                                   file_data.size()) == -1) {
    download_data->output_file.Close();
    return false;
  }

  // Even when writing response to a file save the first 1 MiB of the response
  // body so that it can be used to get error information in case of server side
  // errors. The size limit is to avoid consuming too much redundant memory.
  const size_t kMaxStringSize = 1024 * 1024;
  if (download_data->response_body.size() < kMaxStringSize) {
    size_t bytes_to_copy = std::min(
        file_data.size(), kMaxStringSize - download_data->response_body.size());
    download_data->response_body.append(file_data.data(), bytes_to_copy);
  }

  return true;
}

void UrlFetchRequestBase::OnWriteComplete(
    std::unique_ptr<DownloadData> download_data,
    base::OnceClosure resume,
    bool write_success) {
  download_data_ = std::move(download_data);
  if (!write_success) {
    error_code_ = DRIVE_OTHER_ERROR;
    url_loader_.reset();  // Cancel the request
    // No SimpleURLLoader to call OnComplete() so call it directly.
    OnComplete(false);
    return;
  }

  std::move(resume).Run();
}

void UrlFetchRequestBase::OnDataReceived(base::StringPiece string_piece,
                                         base::OnceClosure resume) {
  if (!download_data_->get_content_callback.is_null()) {
    download_data_->get_content_callback.Run(
        HTTP_SUCCESS, std::make_unique<std::string>(string_piece));
  }

  if (!download_data_->output_file_path.empty()) {
    DownloadData* download_data_ptr = download_data_.get();
    base::PostTaskAndReplyWithResult(
        blocking_task_runner(), FROM_HERE,
        base::BindOnce(&UrlFetchRequestBase::WriteFileData,
                       string_piece.as_string(), download_data_ptr),
        base::BindOnce(&UrlFetchRequestBase::OnWriteComplete,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(download_data_), std::move(resume)));
    return;
  }

  download_data_->response_body.append(string_piece.data(),
                                       string_piece.size());
  std::move(resume).Run();
}

void UrlFetchRequestBase::OnComplete(bool success) {
  DCHECK(download_data_);
  blocking_task_runner()->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&CloseFile, std::move(download_data_->output_file)),
      base::BindOnce(&UrlFetchRequestBase::OnOutputFileClosed,
                     weak_ptr_factory_.GetWeakPtr(), success));
}

void UrlFetchRequestBase::OnOutputFileClosed(bool success) {
  DCHECK(download_data_);
  const network::mojom::URLResponseHead* response_info;
  if (url_loader_) {
    response_info = url_loader_->ResponseInfo();
    if (response_info) {
      error_code_ = static_cast<DriveApiErrorCode>(
          response_info->headers->response_code());
    } else {
      error_code_ = NetError() == net::ERR_NETWORK_CHANGED ? DRIVE_NO_CONNECTION
                                                           : DRIVE_OTHER_ERROR;
    }
    if (!download_data_->response_body.empty()) {
      error_code_ =
          MapJsonError(error_code_.value(), download_data_->response_body);
    }
  } else {
    // If the request is cancelled then error_code_ must be set.
    DCHECK(error_code_.has_value());
    response_info = nullptr;
  }

  if (error_code_.value() == HTTP_UNAUTHORIZED) {
    if (++re_authenticate_count_ <= kMaxReAuthenticateAttemptsPerRequest) {
      // Reset re_authenticate_callback_ so Start() can be called again.
      std::move(re_authenticate_callback_).Run(this);
      return;
    }
    OnAuthFailed(GetErrorCode());
    return;
  }

  // Overridden by each specialization
  ProcessURLFetchResults(response_info,
                         std::move(download_data_->output_file_path),
                         std::move(download_data_->response_body));
}

void UrlFetchRequestBase::OnRetry(base::OnceClosure start_retry) {
  NOTREACHED();
}

std::string UrlFetchRequestBase::GetRequestType() const {
  return "GET";
}

std::vector<std::string> UrlFetchRequestBase::GetExtraRequestHeaders() const {
  return std::vector<std::string>();
}

bool UrlFetchRequestBase::GetContentData(std::string* upload_content_type,
                                         std::string* upload_content) {
  return false;
}

bool UrlFetchRequestBase::GetContentFile(base::FilePath* local_file_path,
                                         int64_t* range_offset,
                                         int64_t* range_length,
                                         std::string* upload_content_type) {
  return false;
}

void UrlFetchRequestBase::GetOutputFilePath(
    base::FilePath* local_file_path,
    GetContentCallback* get_content_callback) {
}

void UrlFetchRequestBase::Cancel() {
  url_loader_.reset();
  CompleteRequestWithError(DRIVE_CANCELLED);
}

DriveApiErrorCode UrlFetchRequestBase::GetErrorCode() const {
  DCHECK(error_code_.has_value()) << "GetErrorCode only valid after "
                                     "resource load complete.";
  return error_code_.value();
}

int UrlFetchRequestBase::NetError() const {
  if (!url_loader_)  // If resource load cancelled?
    return net::ERR_FAILED;
  return url_loader_->NetError();
}

bool UrlFetchRequestBase::CalledOnValidThread() {
  return thread_checker_.CalledOnValidThread();
}

base::SequencedTaskRunner* UrlFetchRequestBase::blocking_task_runner() const {
  return sender_->blocking_task_runner();
}

void UrlFetchRequestBase::OnProcessURLFetchResultsComplete() {
  sender_->RequestFinished(this);
}

void UrlFetchRequestBase::CompleteRequestWithError(DriveApiErrorCode code) {
  RunCallbackOnPrematureFailure(code);
  sender_->RequestFinished(this);
}

void UrlFetchRequestBase::OnAuthFailed(DriveApiErrorCode code) {
  CompleteRequestWithError(code);
}

base::WeakPtr<AuthenticatedRequestInterface>
UrlFetchRequestBase::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

//============================ EntryActionRequest ============================

EntryActionRequest::EntryActionRequest(RequestSender* sender,
                                       const EntryActionCallback& callback)
    : UrlFetchRequestBase(sender, ProgressCallback(), ProgressCallback()),
      callback_(callback) {
  DCHECK(!callback_.is_null());
}

EntryActionRequest::~EntryActionRequest() {}

void EntryActionRequest::ProcessURLFetchResults(
    const network::mojom::URLResponseHead* response_head,
    base::FilePath response_file,
    std::string response_body) {
  callback_.Run(GetErrorCode());
  OnProcessURLFetchResultsComplete();
}

void EntryActionRequest::RunCallbackOnPrematureFailure(DriveApiErrorCode code) {
  callback_.Run(code);
}

//========================= InitiateUploadRequestBase ========================

InitiateUploadRequestBase::InitiateUploadRequestBase(
    RequestSender* sender,
    const InitiateUploadCallback& callback,
    const std::string& content_type,
    int64_t content_length)
    : UrlFetchRequestBase(sender, ProgressCallback(), ProgressCallback()),
      callback_(callback),
      content_type_(content_type),
      content_length_(content_length) {
  DCHECK(!callback_.is_null());
  DCHECK(!content_type_.empty());
  DCHECK_GE(content_length_, 0);
}

InitiateUploadRequestBase::~InitiateUploadRequestBase() {}

void InitiateUploadRequestBase::ProcessURLFetchResults(
    const network::mojom::URLResponseHead* response_head,
    base::FilePath response_file,
    std::string response_body) {
  std::string upload_location;
  if (GetErrorCode() == HTTP_SUCCESS) {
    // Retrieve value of the first "Location" header.
    response_head->headers->EnumerateHeader(nullptr, kUploadResponseLocation,
                                            &upload_location);
  }

  callback_.Run(GetErrorCode(), GURL(upload_location));
  OnProcessURLFetchResultsComplete();
}

void InitiateUploadRequestBase::RunCallbackOnPrematureFailure(
    DriveApiErrorCode code) {
  callback_.Run(code, GURL());
}

std::vector<std::string>
InitiateUploadRequestBase::GetExtraRequestHeaders() const {
  std::vector<std::string> headers;
  headers.push_back(kUploadContentType + content_type_);
  headers.push_back(kUploadContentLength +
                    base::NumberToString(content_length_));
  return headers;
}

//============================ UploadRangeResponse =============================

UploadRangeResponse::UploadRangeResponse()
    : code(HTTP_SUCCESS),
      start_position_received(0),
      end_position_received(0) {
}

UploadRangeResponse::UploadRangeResponse(DriveApiErrorCode code,
                                         int64_t start_position_received,
                                         int64_t end_position_received)
    : code(code),
      start_position_received(start_position_received),
      end_position_received(end_position_received) {}

UploadRangeResponse::~UploadRangeResponse() {
}

//========================== UploadRangeRequestBase ==========================

UploadRangeRequestBase::UploadRangeRequestBase(
    RequestSender* sender,
    const GURL& upload_url,
    const ProgressCallback& progress_callback)
    : UrlFetchRequestBase(sender, progress_callback, ProgressCallback()),
      upload_url_(upload_url) {}

UploadRangeRequestBase::~UploadRangeRequestBase() {}

GURL UploadRangeRequestBase::GetURL() const {
  // This is very tricky to get json from this request. To do that, &alt=json
  // has to be appended not here but in InitiateUploadRequestBase::GetURL().
  return upload_url_;
}

std::string UploadRangeRequestBase::GetRequestType() const {
  return "PUT";
}

void UploadRangeRequestBase::ProcessURLFetchResults(
    const network::mojom::URLResponseHead* response_head,
    base::FilePath response_file,
    std::string response_body) {
  DriveApiErrorCode code = GetErrorCode();
  if (code == HTTP_RESUME_INCOMPLETE) {
    // Retrieve value of the first "Range" header.
    // The Range header is appeared only if there is at least one received
    // byte. So, initialize the positions by 0 so that the [0,0) will be
    // returned via the |callback_| for empty data case.
    int64_t start_position_received = 0;
    int64_t end_position_received = 0;
    std::string range_received;
    response_head->headers->EnumerateHeader(nullptr, kUploadResponseRange,
                                            &range_received);
    if (!range_received.empty()) {  // Parse the range header.
      std::vector<net::HttpByteRange> ranges;
      if (net::HttpUtil::ParseRangeHeader(range_received, &ranges) &&
          !ranges.empty() ) {
        // We only care about the first start-end pair in the range.
        //
        // Range header represents the range inclusively, while we are treating
        // ranges exclusively (i.e., end_position_received should be one passed
        // the last valid index). So "+ 1" is added.
        start_position_received = ranges[0].first_byte_position();
        end_position_received = ranges[0].last_byte_position() + 1;
      }
    }
    // The Range header has the received data range, so the start position
    // should be always 0.
    DCHECK_EQ(start_position_received, 0);

    OnRangeRequestComplete(UploadRangeResponse(code, start_position_received,
                                               end_position_received),
                           std::unique_ptr<base::Value>());

    OnProcessURLFetchResultsComplete();
  } else if (code == HTTP_CREATED || code == HTTP_SUCCESS) {
    // The upload is successfully done. Parse the response which should be
    // the entry's metadata.
    ParseJsonOnBlockingPool(
        blocking_task_runner(), std::move(response_body),
        base::BindOnce(&UploadRangeRequestBase::OnDataParsed,
                       weak_ptr_factory_.GetWeakPtr(), code));
  } else {
    // Failed to upload. Run callbacks to notify the error.
    OnRangeRequestComplete(UploadRangeResponse(code, -1, -1),
                           std::unique_ptr<base::Value>());
    OnProcessURLFetchResultsComplete();
  }
}

void UploadRangeRequestBase::OnDataParsed(DriveApiErrorCode code,
                                          std::unique_ptr<base::Value> value) {
  DCHECK(CalledOnValidThread());
  DCHECK(code == HTTP_CREATED || code == HTTP_SUCCESS);

  OnRangeRequestComplete(UploadRangeResponse(code, -1, -1), std::move(value));
  OnProcessURLFetchResultsComplete();
}

void UploadRangeRequestBase::RunCallbackOnPrematureFailure(
    DriveApiErrorCode code) {
  OnRangeRequestComplete(UploadRangeResponse(code, 0, 0),
                         std::unique_ptr<base::Value>());
}

UploadRangeRequestBase::DownloadData::DownloadData(
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner)
    : blocking_task_runner_(blocking_task_runner) {}

UploadRangeRequestBase::DownloadData::~DownloadData() {
  if (output_file.IsValid()) {
    blocking_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&CloseFile, std::move(output_file)));
  }
}

//========================== ResumeUploadRequestBase =========================

ResumeUploadRequestBase::ResumeUploadRequestBase(
    RequestSender* sender,
    const GURL& upload_location,
    int64_t start_position,
    int64_t end_position,
    int64_t content_length,
    const std::string& content_type,
    const base::FilePath& local_file_path,
    const ProgressCallback& progress_callback)
    : UploadRangeRequestBase(sender, upload_location, progress_callback),
      start_position_(start_position),
      end_position_(end_position),
      content_length_(content_length),
      content_type_(content_type),
      local_file_path_(local_file_path) {
  DCHECK_LE(start_position_, end_position_);
}

ResumeUploadRequestBase::~ResumeUploadRequestBase() {}

std::vector<std::string>
ResumeUploadRequestBase::GetExtraRequestHeaders() const {
  if (content_length_ == 0) {
    // For uploading an empty document, just PUT an empty content.
    DCHECK_EQ(start_position_, 0);
    DCHECK_EQ(end_position_, 0);
    return std::vector<std::string>();
  }

  // The header looks like
  // Content-Range: bytes <start_position>-<end_position>/<content_length>
  // for example:
  // Content-Range: bytes 7864320-8388607/13851821
  // The header takes inclusive range, so we adjust by "end_position - 1".
  DCHECK_GE(start_position_, 0);
  DCHECK_GT(end_position_, 0);
  DCHECK_GE(content_length_, 0);

  std::vector<std::string> headers;
  headers.push_back(std::string(kUploadContentRange) +
                    base::NumberToString(start_position_) + "-" +
                    base::NumberToString(end_position_ - 1) + "/" +
                    base::NumberToString(content_length_));
  return headers;
}

bool ResumeUploadRequestBase::GetContentFile(base::FilePath* local_file_path,
                                             int64_t* range_offset,
                                             int64_t* range_length,
                                             std::string* upload_content_type) {
  if (start_position_ == end_position_) {
    // No content data.
    return false;
  }

  *local_file_path = local_file_path_;
  *range_offset = start_position_;
  *range_length = end_position_ - start_position_;
  *upload_content_type = content_type_;
  return true;
}

//======================== GetUploadStatusRequestBase ========================

GetUploadStatusRequestBase::GetUploadStatusRequestBase(RequestSender* sender,
                                                       const GURL& upload_url,
                                                       int64_t content_length)
    : UploadRangeRequestBase(sender, upload_url, ProgressCallback()),
      content_length_(content_length) {}

GetUploadStatusRequestBase::~GetUploadStatusRequestBase() {}

std::vector<std::string>
GetUploadStatusRequestBase::GetExtraRequestHeaders() const {
  // The header looks like
  // Content-Range: bytes */<content_length>
  // for example:
  // Content-Range: bytes */13851821
  DCHECK_GE(content_length_, 0);

  std::vector<std::string> headers;
  headers.push_back(std::string(kUploadContentRange) + "*/" +
                    base::NumberToString(content_length_));
  return headers;
}

//========================= MultipartUploadRequestBase ========================

MultipartUploadRequestBase::MultipartUploadRequestBase(
    base::SequencedTaskRunner* blocking_task_runner,
    const std::string& metadata_json,
    const std::string& content_type,
    int64_t content_length,
    const base::FilePath& local_file_path,
    const FileResourceCallback& callback,
    const ProgressCallback& progress_callback)
    : blocking_task_runner_(blocking_task_runner),
      metadata_json_(metadata_json),
      content_type_(content_type),
      local_path_(local_file_path),
      callback_(callback),
      progress_callback_(progress_callback) {
  DCHECK(!content_type.empty());
  DCHECK_GE(content_length, 0);
  DCHECK(!local_file_path.empty());
  DCHECK(!callback.is_null());
}

MultipartUploadRequestBase::~MultipartUploadRequestBase() {
}

std::vector<std::string> MultipartUploadRequestBase::GetExtraRequestHeaders()
    const {
  return std::vector<std::string>();
}

void MultipartUploadRequestBase::Prepare(const PrepareCallback& callback) {
  // If the request is cancelled, the request instance will be deleted in
  // |UrlFetchRequestBase::Cancel| and OnPrepareUploadContent won't be called.
  std::string* const upload_content_type = new std::string();
  std::string* const upload_content_data = new std::string();
  PostTaskAndReplyWithResult(
      blocking_task_runner_.get(), FROM_HERE,
      base::Bind(&GetMultipartContent, boundary_, metadata_json_, content_type_,
                 local_path_, base::Unretained(upload_content_type),
                 base::Unretained(upload_content_data)),
      base::Bind(&MultipartUploadRequestBase::OnPrepareUploadContent,
                 weak_ptr_factory_.GetWeakPtr(), callback,
                 base::Owned(upload_content_type),
                 base::Owned(upload_content_data)));
}

void MultipartUploadRequestBase::OnPrepareUploadContent(
    const PrepareCallback& callback,
    std::string* upload_content_type,
    std::string* upload_content_data,
    bool result) {
  if (!result) {
    callback.Run(DRIVE_FILE_ERROR);
    return;
  }
  upload_content_type_.swap(*upload_content_type);
  upload_content_data_.swap(*upload_content_data);
  callback.Run(HTTP_SUCCESS);
}

void MultipartUploadRequestBase::SetBoundaryForTesting(
    const std::string& boundary) {
  boundary_ = boundary;
}

bool MultipartUploadRequestBase::GetContentData(
    std::string* upload_content_type,
    std::string* upload_content_data) {
  // TODO(hirono): Pass stream instead of actual data to reduce memory usage.
  upload_content_type->swap(upload_content_type_);
  upload_content_data->swap(upload_content_data_);
  return true;
}

void MultipartUploadRequestBase::NotifyResult(
    DriveApiErrorCode code,
    const std::string& body,
    const base::Closure& notify_complete_callback) {
  // The upload is successfully done. Parse the response which should be
  // the entry's metadata.
  if (code == HTTP_CREATED || code == HTTP_SUCCESS) {
    ParseJsonOnBlockingPool(
        blocking_task_runner_.get(), body,
        base::BindOnce(&MultipartUploadRequestBase::OnDataParsed,
                       weak_ptr_factory_.GetWeakPtr(), code,
                       notify_complete_callback));
  } else {
    NotifyError(MapJsonError(code, body));
    notify_complete_callback.Run();
  }
}

void MultipartUploadRequestBase::NotifyError(DriveApiErrorCode code) {
  callback_.Run(code, std::unique_ptr<FileResource>());
}

void MultipartUploadRequestBase::NotifyUploadProgress(int64_t current,
                                                      int64_t total) {
  if (!progress_callback_.is_null())
    progress_callback_.Run(current, total);
}

void MultipartUploadRequestBase::OnDataParsed(
    DriveApiErrorCode code,
    const base::Closure& notify_complete_callback,
    std::unique_ptr<base::Value> value) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (value)
    callback_.Run(code, google_apis::FileResource::CreateFrom(*value));
  else
    NotifyError(DRIVE_PARSE_ERROR);
  notify_complete_callback.Run();
}

//============================ DownloadFileRequestBase =========================

DownloadFileRequestBase::DownloadFileRequestBase(
    RequestSender* sender,
    const DownloadActionCallback& download_action_callback,
    const GetContentCallback& get_content_callback,
    const ProgressCallback& progress_callback,
    const GURL& download_url,
    const base::FilePath& output_file_path)
    : UrlFetchRequestBase(sender, ProgressCallback(), progress_callback),
      download_action_callback_(download_action_callback),
      get_content_callback_(get_content_callback),
      download_url_(download_url),
      output_file_path_(output_file_path) {
  DCHECK(!download_action_callback_.is_null());
  DCHECK(!output_file_path_.empty());
  // get_content_callback may be null.
}

DownloadFileRequestBase::~DownloadFileRequestBase() {}

// Overridden from UrlFetchRequestBase.
GURL DownloadFileRequestBase::GetURL() const {
  return download_url_;
}

void DownloadFileRequestBase::GetOutputFilePath(
    base::FilePath* local_file_path,
    GetContentCallback* get_content_callback) {
  // Configure so that the downloaded content is saved to |output_file_path_|.
  *local_file_path = output_file_path_;
  *get_content_callback = get_content_callback_;
}

void DownloadFileRequestBase::ProcessURLFetchResults(
    const network::mojom::URLResponseHead* response_head,
    base::FilePath response_file,
    std::string response_body) {
  download_action_callback_.Run(GetErrorCode(), response_file);
  OnProcessURLFetchResultsComplete();
}

void DownloadFileRequestBase::RunCallbackOnPrematureFailure(
    DriveApiErrorCode code) {
  download_action_callback_.Run(code, base::FilePath());
}

}  // namespace google_apis
