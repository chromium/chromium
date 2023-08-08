// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/classroom/classroom_api_list_course_work_request.h"

#include <memory>
#include <string>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/strings/string_util.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "google_apis/classroom/classroom_api_course_work_response_types.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/base_requests.h"
#include "google_apis/common/request_sender.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace google_apis::classroom {
namespace {

constexpr char kListCourseWorkUrlTemplate[] = "/v1/courses/$1/courseWork";

constexpr char kFieldsParameterName[] = "fields";
constexpr char kRequestedFields[] =
    "courseWork(id,title,state,alternateLink,creationTime,updateTime,"
    "dueDate(year,month,day),dueTime(hours,minutes,seconds,nanos)),"
    "nextPageToken";

constexpr char kPageTokenParameterName[] = "pageToken";

std::unique_ptr<CourseWork> ParseResponse(std::string json) {
  std::unique_ptr<base::Value> raw_value = ParseJson(json);
  return raw_value ? CourseWork::CreateFrom(*raw_value) : nullptr;
}

}  // namespace

ListCourseWorkRequest::ListCourseWorkRequest(RequestSender* sender,
                                             const std::string& course_id,
                                             const std::string& page_token,
                                             Callback callback)
    : UrlFetchRequestBase(sender, ProgressCallback(), ProgressCallback()),
      course_id_(course_id),
      page_token_(page_token),
      callback_(std::move(callback)) {
  CHECK(!course_id_.empty());
  CHECK(!callback_.is_null());
}

ListCourseWorkRequest::~ListCourseWorkRequest() = default;

GURL ListCourseWorkRequest::GetURL() const {
  auto url = GaiaUrls::GetInstance()->classroom_api_origin_url().Resolve(
      base::ReplaceStringPlaceholders(kListCourseWorkUrlTemplate, {course_id_},
                                      nullptr));
  url = net::AppendOrReplaceQueryParameter(url, kFieldsParameterName,
                                           kRequestedFields);
  if (!page_token_.empty()) {
    url = net::AppendOrReplaceQueryParameter(url, kPageTokenParameterName,
                                             page_token_);
  }
  return url;
}

ApiErrorCode ListCourseWorkRequest::MapReasonToError(
    ApiErrorCode code,
    const std::string& reason) {
  return code;
}

bool ListCourseWorkRequest::IsSuccessfulErrorCode(ApiErrorCode error) {
  return error == HTTP_SUCCESS;
}

void ListCourseWorkRequest::ProcessURLFetchResults(
    const network::mojom::URLResponseHead* response_head,
    base::FilePath response_file,
    std::string response_body) {
  ApiErrorCode error = GetErrorCode();
  switch (error) {
    case HTTP_SUCCESS:
      blocking_task_runner()->PostTaskAndReplyWithResult(
          FROM_HERE, base::BindOnce(&ParseResponse, std::move(response_body)),
          base::BindOnce(&ListCourseWorkRequest::OnDataParsed,
                         weak_ptr_factory_.GetWeakPtr()));
      break;
    default:
      RunCallbackOnPrematureFailure(error);
      OnProcessURLFetchResultsComplete();
      break;
  }
}

void ListCourseWorkRequest::RunCallbackOnPrematureFailure(ApiErrorCode error) {
  std::move(callback_).Run(base::unexpected(error));
}

void ListCourseWorkRequest::OnDataParsed(
    std::unique_ptr<CourseWork> course_work) {
  if (!course_work) {
    std::move(callback_).Run(base::unexpected(PARSE_ERROR));
  } else {
    std::move(callback_).Run(std::move(course_work));
  }
  OnProcessURLFetchResultsComplete();
}

}  // namespace google_apis::classroom
