// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/classroom/classroom_api_list_courses_request.h"

#include <memory>
#include <string>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "google_apis/classroom/classroom_api_courses_response_types.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/base_requests.h"
#include "google_apis/common/request_sender.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace google_apis::classroom {
namespace {

constexpr char kListCoursesUrlPath[] = "v1/courses";

constexpr char kFieldsParameterName[] = "fields";
constexpr char kRequestedFields[] =
    "courses(id,name,courseState),nextPageToken";

constexpr char kCourseStatesParameterName[] = "courseStates";
constexpr char kPageTokenParameterName[] = "pageToken";
constexpr char kStudentIdParameterName[] = "studentId";
constexpr char kTeacherIdParameterName[] = "teacherId";

std::unique_ptr<Courses> ParseResponse(std::string json) {
  std::unique_ptr<base::Value> raw_value = ParseJson(json);
  return raw_value ? Courses::CreateFrom(*raw_value) : nullptr;
}

}  // namespace

ListCoursesRequest::ListCoursesRequest(RequestSender* sender,
                                       const std::string& student_id,
                                       const std::string& teacher_id,
                                       const std::string& page_token,
                                       Callback callback)
    : UrlFetchRequestBase(sender, ProgressCallback(), ProgressCallback()),
      student_id_(student_id),
      teacher_id_(teacher_id),
      page_token_(page_token),
      callback_(std::move(callback)) {
  CHECK(!student_id_.empty() || !teacher_id_.empty());
  CHECK(!callback_.is_null());
}

ListCoursesRequest::~ListCoursesRequest() = default;

GURL ListCoursesRequest::GetURL() const {
  auto url = GaiaUrls::GetInstance()->classroom_api_origin_url().Resolve(
      kListCoursesUrlPath);
  url = net::AppendOrReplaceQueryParameter(url, kFieldsParameterName,
                                           kRequestedFields);
  if (!student_id_.empty()) {
    url = net::AppendOrReplaceQueryParameter(url, kStudentIdParameterName,
                                             student_id_);
  }
  if (!teacher_id_.empty()) {
    url = net::AppendOrReplaceQueryParameter(url, kTeacherIdParameterName,
                                             teacher_id_);
  }
  url = net::AppendOrReplaceQueryParameter(
      url, kCourseStatesParameterName,
      Course::StateToString(Course::State::kActive));
  if (!page_token_.empty()) {
    url = net::AppendOrReplaceQueryParameter(url, kPageTokenParameterName,
                                             page_token_);
  }
  return url;
}

ApiErrorCode ListCoursesRequest::MapReasonToError(ApiErrorCode code,
                                                  const std::string& reason) {
  return code;
}

bool ListCoursesRequest::IsSuccessfulErrorCode(ApiErrorCode error) {
  return error == HTTP_SUCCESS;
}

void ListCoursesRequest::ProcessURLFetchResults(
    const network::mojom::URLResponseHead* response_head,
    base::FilePath response_file,
    std::string response_body) {
  ApiErrorCode error = GetErrorCode();
  switch (error) {
    case HTTP_SUCCESS:
      blocking_task_runner()->PostTaskAndReplyWithResult(
          FROM_HERE, base::BindOnce(&ParseResponse, std::move(response_body)),
          base::BindOnce(&ListCoursesRequest::OnDataParsed,
                         weak_ptr_factory_.GetWeakPtr()));
      break;
    default:
      RunCallbackOnPrematureFailure(error);
      OnProcessURLFetchResultsComplete();
      break;
  }
}

void ListCoursesRequest::RunCallbackOnPrematureFailure(ApiErrorCode error) {
  std::move(callback_).Run(base::unexpected(error));
}

void ListCoursesRequest::OnDataParsed(std::unique_ptr<Courses> courses) {
  if (!courses) {
    std::move(callback_).Run(base::unexpected(PARSE_ERROR));
  } else {
    std::move(callback_).Run(std::move(courses));
  }
  OnProcessURLFetchResultsComplete();
}

}  // namespace google_apis::classroom
