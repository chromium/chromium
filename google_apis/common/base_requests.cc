// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "google_apis/common/base_requests.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "google_apis/common/request_sender.h"
#include "google_apis/common/task_util.h"
#include "google_apis/credentials_mode.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace {

// Template for optional OAuth2 authorization HTTP header.
const char kAuthorizationHeaderFormat[] = "Authorization: Bearer %s";
// Template for GData API version HTTP header.
const char kGDataVersionHeader[] = "GData-Version: 3.0";

// Maximum number of attempts for re-authentication per request.
const int kMaxReAuthenticateAttemptsPerRequest = 1;

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

}  // namespace

namespace google_apis {

std::optional<std::string> MapJsonErrorToReason(const std::string& error_body) {
  DVLOG(1) << error_body;
  const char kErrorKey[] = "error";
  const char kErrorErrorsKey[] = "errors";
  const char kErrorReasonKey[] = "reason";
  const char kErrorMessageKey[] = "message";
  const char kErrorCodeKey[] = "code";

  std::unique_ptr<const base::Value> value(google_apis::ParseJson(error_body));
  const base::Value::Dict* dictionary = value ? value->GetIfDict() : nullptr;
  const base::Value::Dict* error =
      dictionary ? dictionary->FindDict(kErrorKey) : nullptr;
  if (error) {
    // Get error message and code.
    const std::string* message = error->FindString(kErrorMessageKey);
    std::optional<int> code = error->FindInt(kErrorCodeKey);
    DLOG(ERROR) << "code: " << (code ? code.value() : OTHER_ERROR)
                << ", message: " << (message ? *message : "");

    // Returns the reason of the first error.
    if (const base::Value::List* errors = error->FindList(kErrorErrorsKey)) {
      const base::Value::Dict* first_error = errors->front().GetIfDict();
      if (first_error) {
        const std::string* reason = first_error->FindString(kErrorReasonKey);
        if (reason) {
          return *reason;
        }
      }
    }
  }
  return std::nullopt;
}

std::unique_ptr<base::Value> ParseJson(const std::string& json) {
  auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(json);
  if (!parsed_json.has_value()) {
    std::string trimmed_json;
    if (json.size() < 80) {
      trimmed_json = json;
    } else {
      // Take the first 50 and the last 10 bytes.
      trimmed_json =
          base::StringPrintf("%s [%s bytes] %s", json.substr(0, 50).c_str(),
                             base::NumberToString(json.size() - 60).c_str(),
                             json.substr(json.size() - 10).c_str());
    }
    LOG(WARNING) << "Error while parsing entry response: "
                 << parsed_json.error().message << ", json:\n"
                 << trimmed_json;
    return nullptr;
  }
  return base::Value::ToUniquePtrValue(std::move(*parsed_json));
}

std::string HttpRequestMethodToString(HttpRequestMethod method) {
  switch (method) {
    case HttpRequestMethod::kGet:
      return net::HttpRequestHeaders::kGetMethod;
    case HttpRequestMethod::kPost:
      return net::HttpRequestHeaders::kPostMethod;
    case HttpRequestMethod::kPut:
      return net::HttpRequestHeaders::kPutMethod;
    case HttpRequestMethod::kPatch:
      return net::HttpRequestHeaders::kPatchMethod;
    case HttpRequestMethod::kDelete:
      return net::HttpRequestHeaders::kDeleteMethod;
  }
}

UrlFetchRequestBase::UrlFetchRequestBase(
    RequestSender* sender,
    ProgressCallback upload_progress_callback,
    ProgressCallback download_progress_callback)
    : re_authenticate_count_(0),
      sender_(sender),
      upload_progress_callback_(upload_progress_callback),
      download_progress_callback_(download_progress_callback),
      response_content_length_(-1) {}

UrlFetchRequestBase::~UrlFetchRequestBase() = default;

void UrlFetchRequestBase::Start(const std::string& access_token,
                                const std::string& custom_user_agent,
                                ReAuthenticateCallback callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(!access_token.empty());
  DCHECK(callback);
  DCHECK(re_authenticate_callback_.is_null());
  Prepare(base::BindOnce(&UrlFetchRequestBase::StartAfterPrepare,
                         weak_ptr_factory_.GetWeakPtr(), access_token,
                         custom_user_agent, std::move(callback)));
}

void UrlFetchRequestBase::Prepare(PrepareCallback callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(!callback.is_null());
  std::move(callback).Run(HTTP_SUCCESS);
}

void UrlFetchRequestBase::StartAfterPrepare(
    const std::string& access_token,
    const std::string& custom_user_agent,
    ReAuthenticateCallback callback,
    ApiErrorCode code) {
  DCHECK(CalledOnValidThread());
  DCHECK(!access_token.empty());
  DCHECK(callback);
  DCHECK(re_authenticate_callback_.is_null());

  GURL url = GetURL();
  ApiErrorCode error_code;
  if (IsSuccessfulErrorCode(code))
    error_code = code;
  else if (url.is_empty())
    error_code = OTHER_ERROR;
  else
    error_code = HTTP_SUCCESS;

  if (error_code != HTTP_SUCCESS) {
    // Error is found on generating the url or preparing the request. Send the
    // error message to the callback, and then return immediately without trying
    // to connect to the server.  We need to call CompleteRequestWithError
    // asynchronously because client code does not assume result callback is
    // called synchronously.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&UrlFetchRequestBase::CompleteRequestWithError,
                       weak_ptr_factory_.GetWeakPtr(), error_code));
    return;
  }

  re_authenticate_callback_ = callback;
  DVLOG(1) << "URL: " << url.spec();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = std::move(url);
  request->method = HttpRequestMethodToString(GetRequestType());
  request->load_flags = net::LOAD_DISABLE_CACHE;
  request->credentials_mode = GetOmitCredentialsModeForGaiaRequests();

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

void UrlFetchRequestBase::OnDownloadProgress(ProgressCallback progress_callback,
                                             uint64_t current) {
  progress_callback.Run(static_cast<int64_t>(current),
                        response_content_length_);
}

void UrlFetchRequestBase::OnUploadProgress(ProgressCallback progress_callback,
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

UrlFetchRequestBase::DownloadData::DownloadData(
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner)
    : blocking_task_runner_(blocking_task_runner) {}

UrlFetchRequestBase::DownloadData::~DownloadData() {
  if (output_file.IsValid()) {
    blocking_task_runner_->PostTask(
        FROM_HERE, base::DoNothingWithBoundArgs(std::move(output_file)));
  }
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
  if (!download_data->output_file.WriteAtCurrentPosAndCheck(
          base::as_byte_span(file_data))) {
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
    error_code_ = OTHER_ERROR;
    url_loader_.reset();  // Cancel the request
    // No SimpleURLLoader to call OnComplete() so call it directly.
    OnComplete(false);
    return;
  }

  std::move(resume).Run();
}

void UrlFetchRequestBase::OnDataReceived(std::string_view string_piece,
                                         base::OnceClosure resume) {
  if (!download_data_->get_content_callback.is_null()) {
    download_data_->get_content_callback.Run(
        HTTP_SUCCESS, std::make_unique<std::string>(string_piece),
        download_data_->response_body.empty());
  }

  if (!download_data_->output_file_path.empty()) {
    DownloadData* download_data_ptr = download_data_.get();
    blocking_task_runner()->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&UrlFetchRequestBase::WriteFileData,
                       std::string(string_piece), download_data_ptr),
        base::BindOnce(&UrlFetchRequestBase::OnWriteComplete,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(download_data_), std::move(resume)));
    return;
  }

  download_data_->response_body.append(string_piece);
  std::move(resume).Run();
}

void UrlFetchRequestBase::OnComplete(bool success) {
  DCHECK(download_data_);
  blocking_task_runner()->PostTaskAndReply(
      FROM_HERE,
      base::DoNothingWithBoundArgs(std::move(download_data_->output_file)),
      base::BindOnce(&UrlFetchRequestBase::OnOutputFileClosed,
                     weak_ptr_factory_.GetWeakPtr(), success));
}

void UrlFetchRequestBase::OnOutputFileClosed(bool success) {
  DCHECK(download_data_);
  const network::mojom::URLResponseHead* response_info;
  if (url_loader_) {
    response_info = url_loader_->ResponseInfo();
    if (response_info) {
      error_code_ =
          static_cast<ApiErrorCode>(response_info->headers->response_code());
    } else {
      error_code_ =
          NetError() == net::ERR_NETWORK_CHANGED ? NO_CONNECTION : OTHER_ERROR;
    }
    if (!download_data_->response_body.empty()) {
      if (!IsSuccessfulErrorCode(error_code_.value())) {
        std::optional<std::string> reason =
            MapJsonErrorToReason(download_data_->response_body);
        if (reason.has_value())
          error_code_ = MapReasonToError(error_code_.value(), reason.value());
      }
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

HttpRequestMethod UrlFetchRequestBase::GetRequestType() const {
  return HttpRequestMethod::kGet;
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
    GetContentCallback* get_content_callback) {}

void UrlFetchRequestBase::Cancel() {
  url_loader_.reset();
  CompleteRequestWithError(CANCELLED);
}

ApiErrorCode UrlFetchRequestBase::GetErrorCode() const {
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

void UrlFetchRequestBase::CompleteRequestWithError(ApiErrorCode code) {
  RunCallbackOnPrematureFailure(code);
  sender_->RequestFinished(this);
}

void UrlFetchRequestBase::OnAuthFailed(ApiErrorCode code) {
  CompleteRequestWithError(code);
}

base::WeakPtr<AuthenticatedRequestInterface> UrlFetchRequestBase::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace google_apis
