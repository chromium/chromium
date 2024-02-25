// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/drive/drive_base_requests.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "google_apis/common/base_requests.h"
#include "google_apis/common/request_sender.h"
#include "google_apis/common/task_util.h"
#include "google_apis/common/time_util.h"
#include "google_apis/drive/drive_api_parser.h"
#include "google_apis/drive/request_util.h"
#include "net/base/load_flags.h"
#include "net/base/mime_util.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace {

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
const char kMultipartRelatedMimeTypePrefix[] = "multipart/related; boundary=";

// Mime type of multipart mixed.
const char kMultipartMixedMimeTypePrefix[] = "multipart/mixed; boundary=";

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
  blocking_task_runner->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&google_apis::ParseJson, std::move(json)),
      std::move(callback));
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
  GenerateMultipartBody(google_apis::MultipartType::kRelated,
                        predetermined_boundary, parts, &output, nullptr);
  upload_content_type->swap(output.type);
  upload_content_data->swap(output.data);
  return true;
}

// See https://developers.google.com/drive/handle-errors
google_apis::ApiErrorCode MapDriveReasonToError(google_apis::ApiErrorCode code,
                                                const std::string& reason) {
  const char kErrorReasonRateLimitExceeded[] = "rateLimitExceeded";
  const char kErrorReasonUserRateLimitExceeded[] = "userRateLimitExceeded";
  const char kErrorReasonQuotaExceeded[] = "quotaExceeded";
  const char kErrorReasonResponseTooLarge[] = "responseTooLarge";

  if (reason == kErrorReasonRateLimitExceeded ||
      reason == kErrorReasonUserRateLimitExceeded) {
    return google_apis::HTTP_SERVICE_UNAVAILABLE;
  }
  if (reason == kErrorReasonQuotaExceeded)
    return google_apis::DRIVE_NO_SPACE;
  if (reason == kErrorReasonResponseTooLarge)
    return google_apis::DRIVE_RESPONSE_TOO_LARGE;
  return code;
}

}  // namespace

namespace google_apis {

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
      for (const auto& part : parts) {
        if (base::Contains(part.data, boundary)) {
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
    case MultipartType::kRelated:
      output->type = kMultipartRelatedMimeTypePrefix + boundary;
      break;
    case MultipartType::kMixed:
      output->type = kMultipartMixedMimeTypePrefix + boundary;
      break;
  }

  output->data.clear();
  if (data_offset)
    data_offset->clear();
  for (const auto& part : parts) {
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

//========================== DriveUrlFetchRequestBase ======================
DriveUrlFetchRequestBase::DriveUrlFetchRequestBase(
    RequestSender* sender,
    ProgressCallback upload_progress_callback,
    ProgressCallback download_progress_callback)
    : UrlFetchRequestBase(sender,
                          upload_progress_callback,
                          download_progress_callback) {}

DriveUrlFetchRequestBase::~DriveUrlFetchRequestBase() = default;

ApiErrorCode DriveUrlFetchRequestBase::MapReasonToError(
    ApiErrorCode code,
    const std::string& reason) {
  return MapDriveReasonToError(code, reason);
}

bool DriveUrlFetchRequestBase::IsSuccessfulErrorCode(ApiErrorCode error) {
  return IsSuccessfulDriveApiErrorCode(error);
}

//============================ EntryActionRequest ============================

EntryActionRequest::EntryActionRequest(RequestSender* sender,
                                       EntryActionCallback callback)
    : DriveUrlFetchRequestBase(sender, ProgressCallback(), ProgressCallback()),
      callback_(std::move(callback)) {
  DCHECK(callback_);
}

EntryActionRequest::~EntryActionRequest() = default;

void EntryActionRequest::ProcessURLFetchResults(
    const network::mojom::URLResponseHead* response_head,
    base::FilePath response_file,
    std::string response_body) {
  std::move(callback_).Run(GetErrorCode());
  OnProcessURLFetchResultsComplete();
}

void EntryActionRequest::RunCallbackOnPrematureFailure(ApiErrorCode code) {
  std::move(callback_).Run(code);
}

//========================= InitiateUploadRequestBase ========================

InitiateUploadRequestBase::InitiateUploadRequestBase(
    RequestSender* sender,
    InitiateUploadCallback callback,
    const std::string& content_type,
    int64_t content_length)
    : DriveUrlFetchRequestBase(sender, ProgressCallback(), ProgressCallback()),
      callback_(std::move(callback)),
      content_type_(content_type),
      content_length_(content_length) {
  DCHECK(!callback_.is_null());
  DCHECK(!content_type_.empty());
  DCHECK_GE(content_length_, 0);
}

InitiateUploadRequestBase::~InitiateUploadRequestBase() = default;

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

  std::move(callback_).Run(GetErrorCode(), GURL(upload_location));
  OnProcessURLFetchResultsComplete();
}

void InitiateUploadRequestBase::RunCallbackOnPrematureFailure(
    ApiErrorCode code) {
  std::move(callback_).Run(code, GURL());
}

std::vector<std::string> InitiateUploadRequestBase::GetExtraRequestHeaders()
    const {
  std::vector<std::string> headers;
  headers.push_back(kUploadContentType + content_type_);
  headers.push_back(kUploadContentLength +
                    base::NumberToString(content_length_));
  return headers;
}

//============================ UploadRangeResponse =============================

UploadRangeResponse::UploadRangeResponse() = default;

UploadRangeResponse::UploadRangeResponse(ApiErrorCode code,
                                         int64_t start_position_received,
                                         int64_t end_position_received)
    : code(code),
      start_position_received(start_position_received),
      end_position_received(end_position_received) {}

UploadRangeResponse::~UploadRangeResponse() = default;

//========================== UploadRangeRequestBase ==========================

UploadRangeRequestBase::UploadRangeRequestBase(
    RequestSender* sender,
    const GURL& upload_url,
    ProgressCallback progress_callback)
    : DriveUrlFetchRequestBase(sender, progress_callback, ProgressCallback()),
      upload_url_(upload_url) {}

UploadRangeRequestBase::~UploadRangeRequestBase() = default;

GURL UploadRangeRequestBase::GetURL() const {
  // This is very tricky to get json from this request. To do that, &alt=json
  // has to be appended not here but in InitiateUploadRequestBase::GetURL().
  return upload_url_;
}

HttpRequestMethod UploadRangeRequestBase::GetRequestType() const {
  return HttpRequestMethod::kPut;
}

void UploadRangeRequestBase::ProcessURLFetchResults(
    const network::mojom::URLResponseHead* response_head,
    base::FilePath response_file,
    std::string response_body) {
  ApiErrorCode code = GetErrorCode();
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
          !ranges.empty()) {
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

void UploadRangeRequestBase::OnDataParsed(ApiErrorCode code,
                                          std::unique_ptr<base::Value> value) {
  DCHECK(CalledOnValidThread());
  DCHECK(code == HTTP_CREATED || code == HTTP_SUCCESS);

  OnRangeRequestComplete(UploadRangeResponse(code, -1, -1), std::move(value));
  OnProcessURLFetchResultsComplete();
}

void UploadRangeRequestBase::RunCallbackOnPrematureFailure(ApiErrorCode code) {
  OnRangeRequestComplete(UploadRangeResponse(code, 0, 0),
                         std::unique_ptr<base::Value>());
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
    ProgressCallback progress_callback)
    : UploadRangeRequestBase(sender, upload_location, progress_callback),
      start_position_(start_position),
      end_position_(end_position),
      content_length_(content_length),
      content_type_(content_type),
      local_file_path_(local_file_path) {
  DCHECK_LE(start_position_, end_position_);
}

ResumeUploadRequestBase::~ResumeUploadRequestBase() = default;

std::vector<std::string> ResumeUploadRequestBase::GetExtraRequestHeaders()
    const {
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

GetUploadStatusRequestBase::~GetUploadStatusRequestBase() = default;

std::vector<std::string> GetUploadStatusRequestBase::GetExtraRequestHeaders()
    const {
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
    FileResourceCallback callback,
    ProgressCallback progress_callback)
    : blocking_task_runner_(blocking_task_runner),
      metadata_json_(metadata_json),
      content_type_(content_type),
      local_path_(local_file_path),
      callback_(std::move(callback)),
      progress_callback_(progress_callback) {
  DCHECK(!content_type.empty());
  DCHECK_GE(content_length, 0);
  DCHECK(!local_file_path.empty());
  DCHECK(!callback_.is_null());
}

MultipartUploadRequestBase::~MultipartUploadRequestBase() = default;

std::vector<std::string> MultipartUploadRequestBase::GetExtraRequestHeaders()
    const {
  return std::vector<std::string>();
}

void MultipartUploadRequestBase::Prepare(PrepareCallback callback) {
  // If the request is cancelled, the request instance will be deleted in
  // |UrlFetchRequestBase::Cancel| and OnPrepareUploadContent won't be called.
  std::string* const upload_content_type = new std::string();
  std::string* const upload_content_data = new std::string();
  blocking_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&GetMultipartContent, boundary_, metadata_json_,
                     content_type_, local_path_,
                     base::Unretained(upload_content_type),
                     base::Unretained(upload_content_data)),
      base::BindOnce(&MultipartUploadRequestBase::OnPrepareUploadContent,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     base::Owned(upload_content_type),
                     base::Owned(upload_content_data)));
}

void MultipartUploadRequestBase::OnPrepareUploadContent(
    PrepareCallback callback,
    std::string* upload_content_type,
    std::string* upload_content_data,
    bool result) {
  if (!result) {
    std::move(callback).Run(DRIVE_FILE_ERROR);
    return;
  }
  upload_content_type_.swap(*upload_content_type);
  upload_content_data_.swap(*upload_content_data);
  std::move(callback).Run(HTTP_SUCCESS);
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
    ApiErrorCode code,
    const std::string& body,
    base::OnceClosure notify_complete_callback) {
  // The upload is successfully done. Parse the response which should be
  // the entry's metadata.
  if (code == HTTP_CREATED || code == HTTP_SUCCESS) {
    ParseJsonOnBlockingPool(
        blocking_task_runner_.get(), body,
        base::BindOnce(&MultipartUploadRequestBase::OnDataParsed,
                       weak_ptr_factory_.GetWeakPtr(), code,
                       std::move(notify_complete_callback)));
  } else {
    std::optional<std::string> reason = MapJsonErrorToReason(body);
    NotifyError(reason.has_value() ? MapDriveReasonToError(code, reason.value())
                                   : code);
    std::move(notify_complete_callback).Run();
  }
}

void MultipartUploadRequestBase::NotifyError(ApiErrorCode code) {
  std::move(callback_).Run(code, std::unique_ptr<FileResource>());
}

void MultipartUploadRequestBase::NotifyUploadProgress(int64_t current,
                                                      int64_t total) {
  if (!progress_callback_.is_null())
    progress_callback_.Run(current, total);
}

void MultipartUploadRequestBase::OnDataParsed(
    ApiErrorCode code,
    base::OnceClosure notify_complete_callback,
    std::unique_ptr<base::Value> value) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (value)
    std::move(callback_).Run(code,
                             google_apis::FileResource::CreateFrom(*value));
  else
    NotifyError(PARSE_ERROR);
  std::move(notify_complete_callback).Run();
}

//============================ DownloadFileRequestBase =========================

DownloadFileRequestBase::DownloadFileRequestBase(
    RequestSender* sender,
    DownloadActionCallback download_action_callback,
    const GetContentCallback& get_content_callback,
    ProgressCallback progress_callback,
    const GURL& download_url,
    const base::FilePath& output_file_path)
    : DriveUrlFetchRequestBase(sender, ProgressCallback(), progress_callback),
      download_action_callback_(std::move(download_action_callback)),
      get_content_callback_(get_content_callback),
      download_url_(download_url),
      output_file_path_(output_file_path) {
  DCHECK(!download_action_callback_.is_null());
  DCHECK(!output_file_path_.empty());
  // get_content_callback may be null.
}

DownloadFileRequestBase::~DownloadFileRequestBase() = default;

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
  std::move(download_action_callback_).Run(GetErrorCode(), response_file);
  OnProcessURLFetchResultsComplete();
}

void DownloadFileRequestBase::RunCallbackOnPrematureFailure(ApiErrorCode code) {
  std::move(download_action_callback_).Run(code, base::FilePath());
}

}  // namespace google_apis
