// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/classroom/classroom_api_list_students_request.h"

#include <memory>
#include <string>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/strings/escape.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "google_apis/classroom/classroom_api_students_response_types.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/base_requests.h"
#include "google_apis/common/request_sender.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace google_apis::classroom {
namespace {

constexpr char kListStudentsUrlTemplate[] = "/v1/courses/$1/students";

constexpr char kFieldsParameterName[] = "fields";
constexpr char kRequestedFields[] =
    "students(profile(id,name(fullName),emailAddress,photoUrl)),nextPageToken";

constexpr char kPageTokenParameterName[] = "pageToken";

std::unique_ptr<Students> ParseResponse(std::string json) {
  std::unique_ptr<base::Value> raw_value = ParseJson(json);
  return raw_value ? Students::CreateFrom(*raw_value) : nullptr;
}

}  // namespace

ListStudentsRequest::ListStudentsRequest(RequestSender* sender,
                                         const std::string& course_id,
                                         const std::string& page_token,
                                         Callback callback)
    : UrlFetchRequestBase(sender, ProgressCallback(), ProgressCallback()),
      course_id_(base::EscapeAllExceptUnreserved(course_id)),
      page_token_(page_token),
      callback_(std::move(callback)) {
  CHECK(!course_id_.empty());
  CHECK(!callback_.is_null());
}

ListStudentsRequest::~ListStudentsRequest() = default;

GURL ListStudentsRequest::GetURL() const {
  auto url = GaiaUrls::GetInstance()->classroom_api_origin_url().Resolve(
      base::ReplaceStringPlaceholders(kListStudentsUrlTemplate, {course_id_},
                                      nullptr));
  url = net::AppendOrReplaceQueryParameter(url, kFieldsParameterName,
                                           kRequestedFields);
  if (!page_token_.empty()) {
    url = net::AppendOrReplaceQueryParameter(url, kPageTokenParameterName,
                                             page_token_);
  }
  return url;
}

ApiErrorCode ListStudentsRequest::MapReasonToError(ApiErrorCode code,
                                                   const std::string& reason) {
  return code;
}

bool ListStudentsRequest::IsSuccessfulErrorCode(ApiErrorCode error) {
  return error == HTTP_SUCCESS;
}

void ListStudentsRequest::ProcessURLFetchResults(
    const network::mojom::URLResponseHead* response_head,
    base::FilePath response_file,
    std::string response_body) {
  ApiErrorCode error = GetErrorCode();
  switch (error) {
    case HTTP_SUCCESS:
      blocking_task_runner()->PostTaskAndReplyWithResult(
          FROM_HERE, base::BindOnce(&ParseResponse, std::move(response_body)),
          base::BindOnce(&ListStudentsRequest::OnDataParsed,
                         weak_ptr_factory_.GetWeakPtr()));
      break;
    default:
      RunCallbackOnPrematureFailure(error);
      OnProcessURLFetchResultsComplete();
      break;
  }
}

void ListStudentsRequest::RunCallbackOnPrematureFailure(ApiErrorCode error) {
  std::move(callback_).Run(base::unexpected(error));
}

void ListStudentsRequest::OnDataParsed(std::unique_ptr<Students> students) {
  if (!students) {
    std::move(callback_).Run(base::unexpected(PARSE_ERROR));
  } else {
    std::move(callback_).Run(std::move(students));
  }
  OnProcessURLFetchResultsComplete();
}

}  // namespace google_apis::classroom
