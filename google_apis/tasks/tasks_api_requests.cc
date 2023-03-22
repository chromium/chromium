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
#include "google_apis/tasks/tasks_api_response_types.h"
#include "google_apis/tasks/tasks_api_url_generator_utils.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace google_apis::tasks {
namespace {

constexpr int kMaxAllowedMaxResults = 100;

}

// ----- ListTaskListsRequest -----

ListTaskListsRequest::ListTaskListsRequest(RequestSender* sender,
                                           Callback callback,
                                           const std::string& page_token)
    : UrlFetchRequestBase(sender, ProgressCallback(), ProgressCallback()),
      callback_(std::move(callback)),
      page_token_(page_token) {
  DCHECK(!callback_.is_null());
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
                                   Callback callback,
                                   const std::string& task_list_id,
                                   const std::string& page_token)
    : UrlFetchRequestBase(sender, ProgressCallback(), ProgressCallback()),
      callback_(std::move(callback)),
      task_list_id_(task_list_id),
      page_token_(page_token) {
  DCHECK(!callback_.is_null());
  DCHECK(!task_list_id_.empty());
}

ListTasksRequest::~ListTasksRequest() = default;

GURL ListTasksRequest::GetURL() const {
  return GetListTasksUrl(task_list_id_, /*include_completed=*/false,
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

}  // namespace google_apis::tasks
