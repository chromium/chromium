// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/tasks/tasks_api_requests.h"

#include <memory>
#include <string>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/base_requests.h"
#include "google_apis/common/request_sender.h"
#include "google_apis/tasks/tasks_api_request_types.h"
#include "google_apis/tasks/tasks_api_response_types.h"
#include "google_apis/tasks/tasks_api_url_generator_utils.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace google_apis::tasks {
namespace {

constexpr char kContentTypeJson[] = "application/json; charset=utf-8";
constexpr int kMaxAllowedMaxResults = 100;

}

// ----- ListTaskListsRequest -----

ListTaskListsRequest::ListTaskListsRequest(RequestSender* sender,
                                           const std::string& page_token,
                                           Callback callback)
    : UrlFetchRequestBase(sender, ProgressCallback(), ProgressCallback()),
      page_token_(page_token),
      callback_(std::move(callback)) {
  CHECK(!callback_.is_null());
}

ListTaskListsRequest::~ListTaskListsRequest() = default;

GURL ListTaskListsRequest::GetURL() const {
  return GetListTaskListsUrl(kMaxAllowedMaxResults, page_token_);
}

ApiErrorCode ListTaskListsRequest::MapReasonToError(ApiErrorCode code,
                                                    const std::string& reason) {
  return code;
}

bool ListTaskListsRequest::IsSuccessfulErrorCode(ApiErrorCode error) {
  return error == HTTP_SUCCESS;
}

void ListTaskListsRequest::ProcessURLFetchResults(
    const network::mojom::URLResponseHead* response_head,
    base::FilePath response_file,
    std::string response_body) {
  ApiErrorCode error = GetErrorCode();
  switch (error) {
    case HTTP_SUCCESS:
      blocking_task_runner()->PostTaskAndReplyWithResult(
          FROM_HERE,
          base::BindOnce(&ListTaskListsRequest::Parse,
                         std::move(response_body)),
          base::BindOnce(&ListTaskListsRequest::OnDataParsed,
                         weak_ptr_factory_.GetWeakPtr()));
      break;
    default:
      RunCallbackOnPrematureFailure(error);
      OnProcessURLFetchResultsComplete();
      break;
  }
}

void ListTaskListsRequest::RunCallbackOnPrematureFailure(ApiErrorCode error) {
  std::move(callback_).Run(base::unexpected(error));
}

std::unique_ptr<TaskLists> ListTaskListsRequest::Parse(std::string json) {
  std::unique_ptr<base::Value> value = ParseJson(json);
  return value ? TaskLists::CreateFrom(*value) : nullptr;
}

void ListTaskListsRequest::OnDataParsed(std::unique_ptr<TaskLists> task_lists) {
  if (!task_lists) {
    std::move(callback_).Run(base::unexpected(PARSE_ERROR));
  } else {
    std::move(callback_).Run(std::move(task_lists));
  }
  OnProcessURLFetchResultsComplete();
}

// ----- ListTasksRequest -----

ListTasksRequest::ListTasksRequest(RequestSender* sender,
                                   const std::string& task_list_id,
                                   const std::string& page_token,
                                   bool include_assigned,
                                   Callback callback)
    : UrlFetchRequestBase(sender, ProgressCallback(), ProgressCallback()),
      task_list_id_(task_list_id),
      page_token_(page_token),
      include_assigned_(include_assigned),
      callback_(std::move(callback)) {
  CHECK(!callback_.is_null());
  CHECK(!task_list_id_.empty());
}

ListTasksRequest::~ListTasksRequest() = default;

GURL ListTasksRequest::GetURL() const {
  return GetListTasksUrl(task_list_id_, /*include_completed=*/false,
                         /*include_assigned=*/include_assigned_,
                         kMaxAllowedMaxResults, page_token_);
}

ApiErrorCode ListTasksRequest::MapReasonToError(ApiErrorCode code,
                                                const std::string& reason) {
  return code;
}

bool ListTasksRequest::IsSuccessfulErrorCode(ApiErrorCode error) {
  return error == HTTP_SUCCESS;
}

void ListTasksRequest::ProcessURLFetchResults(
    const network::mojom::URLResponseHead* response_head,
    base::FilePath response_file,
    std::string response_body) {
  ApiErrorCode error = GetErrorCode();
  switch (error) {
    case HTTP_SUCCESS:
      blocking_task_runner()->PostTaskAndReplyWithResult(
          FROM_HERE,
          base::BindOnce(&ListTasksRequest::Parse, std::move(response_body)),
          base::BindOnce(&ListTasksRequest::OnDataParsed,
                         weak_ptr_factory_.GetWeakPtr()));
      break;
    default:
      RunCallbackOnPrematureFailure(error);
      OnProcessURLFetchResultsComplete();
      break;
  }
}

void ListTasksRequest::RunCallbackOnPrematureFailure(ApiErrorCode error) {
  std::move(callback_).Run(base::unexpected(error));
}

std::unique_ptr<Tasks> ListTasksRequest::Parse(std::string json) {
  std::unique_ptr<base::Value> value = ParseJson(json);
  return value ? Tasks::CreateFrom(*value) : nullptr;
}

void ListTasksRequest::OnDataParsed(std::unique_ptr<Tasks> tasks) {
  if (!tasks) {
    std::move(callback_).Run(base::unexpected(PARSE_ERROR));
  } else {
    std::move(callback_).Run(std::move(tasks));
  }
  OnProcessURLFetchResultsComplete();
}

// ----- PatchTaskRequest -----

PatchTaskRequest::PatchTaskRequest(RequestSender* sender,
                                   const std::string& task_list_id,
                                   const std::string& task_id,
                                   const TaskRequestPayload& payload,
                                   Callback callback)
    : UrlFetchRequestBase(sender, ProgressCallback(), ProgressCallback()),
      task_list_id_(task_list_id),
      task_id_(task_id),
      payload_(payload),
      callback_(std::move(callback)) {
  CHECK(!callback_.is_null());
  CHECK(!task_list_id_.empty());
  CHECK(!task_id_.empty());
}

PatchTaskRequest::~PatchTaskRequest() = default;

GURL PatchTaskRequest::GetURL() const {
  return GetPatchTaskUrl(task_list_id_, task_id_);
}

ApiErrorCode PatchTaskRequest::MapReasonToError(ApiErrorCode code,
                                                const std::string& reason) {
  return code;
}

bool PatchTaskRequest::IsSuccessfulErrorCode(ApiErrorCode error) {
  return error == HTTP_SUCCESS;
}

HttpRequestMethod PatchTaskRequest::GetRequestType() const {
  return HttpRequestMethod::kPatch;
}

bool PatchTaskRequest::GetContentData(std::string* upload_content_type,
                                      std::string* upload_content) {
  *upload_content_type = kContentTypeJson;
  *upload_content = payload_.ToJson();
  return true;
}

void PatchTaskRequest::ProcessURLFetchResults(
    const network::mojom::URLResponseHead* response_head,
    base::FilePath response_file,
    std::string response_body) {
  ApiErrorCode error = GetErrorCode();
  switch (error) {
    case HTTP_SUCCESS:
      blocking_task_runner()->PostTaskAndReplyWithResult(
          FROM_HERE,
          base::BindOnce(&PatchTaskRequest::Parse, std::move(response_body)),
          base::BindOnce(&PatchTaskRequest::OnDataParsed,
                         weak_ptr_factory_.GetWeakPtr()));
      break;
    default:
      RunCallbackOnPrematureFailure(error);
      OnProcessURLFetchResultsComplete();
      break;
  }
}

void PatchTaskRequest::RunCallbackOnPrematureFailure(ApiErrorCode error) {
  std::move(callback_).Run(base::unexpected(error));
}

// static
std::unique_ptr<Task> PatchTaskRequest::Parse(std::string json) {
  std::unique_ptr<base::Value> value = ParseJson(json);
  return value ? Task::CreateFrom(*value) : nullptr;
}

void PatchTaskRequest::OnDataParsed(std::unique_ptr<Task> task) {
  if (!task) {
    std::move(callback_).Run(base::unexpected(PARSE_ERROR));
  } else {
    std::move(callback_).Run(std::move(task));
  }
  OnProcessURLFetchResultsComplete();
}

// ----- InsertTaskRequest -----

InsertTaskRequest::InsertTaskRequest(RequestSender* sender,
                                     const std::string& task_list_id,
                                     const std::string& previous_task_id,
                                     const TaskRequestPayload& payload,
                                     Callback callback)
    : UrlFetchRequestBase(sender, ProgressCallback(), ProgressCallback()),
      task_list_id_(task_list_id),
      previous_task_id_(previous_task_id),
      payload_(payload),
      callback_(std::move(callback)) {
  CHECK(!task_list_id_.empty());
  CHECK(callback_);
}

InsertTaskRequest::~InsertTaskRequest() = default;

GURL InsertTaskRequest::GetURL() const {
  return GetInsertTaskUrl(task_list_id_, previous_task_id_);
}

ApiErrorCode InsertTaskRequest::MapReasonToError(ApiErrorCode code,
                                                 const std::string& reason) {
  return code;
}

bool InsertTaskRequest::IsSuccessfulErrorCode(ApiErrorCode error) {
  return error == HTTP_SUCCESS || error == HTTP_CREATED;
}

HttpRequestMethod InsertTaskRequest::GetRequestType() const {
  return HttpRequestMethod::kPost;
}

bool InsertTaskRequest::GetContentData(std::string* upload_content_type,
                                       std::string* upload_content) {
  *upload_content_type = kContentTypeJson;
  *upload_content = payload_.ToJson();
  return true;
}

void InsertTaskRequest::ProcessURLFetchResults(
    const network::mojom::URLResponseHead* response_head,
    base::FilePath response_file,
    std::string response_body) {
  ApiErrorCode error = GetErrorCode();
  switch (error) {
    case HTTP_SUCCESS:
    case HTTP_CREATED:
      blocking_task_runner()->PostTaskAndReplyWithResult(
          FROM_HERE,
          base::BindOnce(&InsertTaskRequest::Parse, std::move(response_body)),
          base::BindOnce(&InsertTaskRequest::OnDataParsed,
                         weak_ptr_factory_.GetWeakPtr()));
      break;
    default:
      RunCallbackOnPrematureFailure(error);
      OnProcessURLFetchResultsComplete();
      break;
  }
}

void InsertTaskRequest::RunCallbackOnPrematureFailure(ApiErrorCode error) {
  std::move(callback_).Run(base::unexpected(error));
}

// static
std::unique_ptr<Task> InsertTaskRequest::Parse(std::string json) {
  std::unique_ptr<base::Value> value = ParseJson(json);
  return value ? Task::CreateFrom(*value) : nullptr;
}

void InsertTaskRequest::OnDataParsed(std::unique_ptr<Task> task) {
  if (!task) {
    std::move(callback_).Run(base::unexpected(PARSE_ERROR));
  } else {
    std::move(callback_).Run(std::move(task));
  }
  OnProcessURLFetchResultsComplete();
}

}  // namespace google_apis::tasks
