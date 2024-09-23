// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/classroom/classroom_api_list_student_submissions_request.h"

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "google_apis/classroom/classroom_api_student_submissions_response_types.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/base_requests.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"

namespace google_apis::classroom {
namespace {

constexpr char kListStudentSubmissionsUrlTemplate[] =
    "/v1/courses/$1/courseWork/$2/studentSubmissions";

constexpr char kFieldsParameterName[] = "fields";
constexpr char kRequestedFields[] =
    "studentSubmissions(id,courseWorkId,updateTime,state,assignedGrade),"
    "nextPageToken";

constexpr char kPageTokenParameterName[] = "pageToken";

std::unique_ptr<StudentSubmissions> ParseResponse(std::string json) {
  std::unique_ptr<base::Value> raw_value = ParseJson(json);
  return raw_value ? StudentSubmissions::CreateFrom(*raw_value) : nullptr;
}

}  // namespace

ListStudentSubmissionsRequest::ListStudentSubmissionsRequest(
    RequestSender* sender,
    const std::string& course_id,
    const std::string& course_work_id,
    const std::string& page_token,
    Callback callback)
    : UrlFetchRequestBase(sender, ProgressCallback(), ProgressCallback()),
      course_id_(base::EscapeAllExceptUnreserved(course_id)),
      course_work_id_(base::EscapeAllExceptUnreserved(course_work_id)),
      page_token_(page_token),
      callback_(std::move(callback)) {
  CHECK(!course_id_.empty());
  CHECK(!course_work_id_.empty());
  CHECK(!callback_.is_null());
}

ListStudentSubmissionsRequest::~ListStudentSubmissionsRequest() = default;

GURL ListStudentSubmissionsRequest::GetURL() const {
  auto url = GaiaUrls::GetInstance()->classroom_api_origin_url().Resolve(
      base::ReplaceStringPlaceholders(kListStudentSubmissionsUrlTemplate,
                                      {course_id_, course_work_id_}, nullptr));
  url = net::AppendOrReplaceQueryParameter(url, kFieldsParameterName,
                                           kRequestedFields);
  if (!page_token_.empty()) {
    url = net::AppendOrReplaceQueryParameter(url, kPageTokenParameterName,
                                             page_token_);
  }
  return url;
}

ApiErrorCode ListStudentSubmissionsRequest::MapReasonToError(
    ApiErrorCode code,
    const std::string& reason) {
  return code;
}

bool ListStudentSubmissionsRequest::IsSuccessfulErrorCode(ApiErrorCode error) {
  return error == HTTP_SUCCESS;
}

void ListStudentSubmissionsRequest::ProcessURLFetchResults(
    const network::mojom::URLResponseHead* response_head,
    base::FilePath response_file,
    std::string response_body) {
  ApiErrorCode error = GetErrorCode();
  switch (error) {
    case HTTP_SUCCESS:
      blocking_task_runner()->PostTaskAndReplyWithResult(
          FROM_HERE, base::BindOnce(&ParseResponse, std::move(response_body)),
          base::BindOnce(&ListStudentSubmissionsRequest::OnDataParsed,
                         weak_ptr_factory_.GetWeakPtr()));
      break;
    default:
      RunCallbackOnPrematureFailure(error);
      OnProcessURLFetchResultsComplete();
      break;
  }
}

void ListStudentSubmissionsRequest::RunCallbackOnPrematureFailure(
    ApiErrorCode error) {
  std::move(callback_).Run(base::unexpected(error));
}

void ListStudentSubmissionsRequest::OnDataParsed(
    std::unique_ptr<StudentSubmissions> submissions) {
  if (!submissions) {
    std::move(callback_).Run(base::unexpected(PARSE_ERROR));
  } else {
    std::move(callback_).Run(std::move(submissions));
  }
  OnProcessURLFetchResultsComplete();
}

}  // namespace google_apis::classroom
