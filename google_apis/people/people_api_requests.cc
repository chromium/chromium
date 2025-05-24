// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/people/people_api_requests.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_value_converter.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/types/expected.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/base_requests.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/people/people_api_request_types.h"
#include "google_apis/people/people_api_response_types.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace google_apis::people {

namespace {

constexpr std::string_view kContentTypeJson = "application/json; charset=utf-8";
// A `personFields` param must be set, but we are not interested in any of the
// fields provided other than `resourceName`. Arbitrarily use metadata.
constexpr std::string_view kCreateContactPath =
    "v1/people:createContact?personFields=metadata";

std::optional<Person> ParsePerson(std::string_view json) {
  std::optional<base::Value> parsed_json = base::JSONReader::Read(json);
  if (!parsed_json.has_value()) {
    return std::nullopt;
  }

  base::JSONValueConverter<Person> converter;
  Person person;
  if (!converter.Convert(*parsed_json, &person)) {
    return std::nullopt;
  }

  return person;
}

}  // namespace

CreateContactRequest::CreateContactRequest(RequestSender* sender,
                                           Contact payload,
                                           Callback callback)
    : UrlFetchRequestBase(sender, ProgressCallback(), ProgressCallback()),
      contact_payload_(std::move(payload).ToDict()),
      callback_(std::move(callback)) {}

CreateContactRequest::~CreateContactRequest() = default;

GURL CreateContactRequest::GetURL() const {
  return GaiaUrls::GetInstance()->people_api_origin_url().Resolve(
      kCreateContactPath);
}

google_apis::ApiErrorCode CreateContactRequest::MapReasonToError(
    ApiErrorCode code,
    const std::string& reason) {
  return code;
}

bool CreateContactRequest::IsSuccessfulErrorCode(ApiErrorCode error) {
  return error == HTTP_SUCCESS;
}

google_apis::HttpRequestMethod CreateContactRequest::GetRequestType() const {
  return HttpRequestMethod::kPost;
}

bool CreateContactRequest::GetContentData(std::string* upload_content_type,
                                          std::string* upload_content) {
  *upload_content_type = kContentTypeJson;

  std::optional<std::string> json = base::WriteJson(contact_payload_);
  // JSON serialisation should never fail as our `base::Value`s should never
  // have deeply nested fields.
  CHECK(json.has_value());
  *upload_content = std::move(*json);

  return true;
}

void CreateContactRequest::ProcessURLFetchResults(
    const network::mojom::URLResponseHead* response_head,
    const base::FilePath response_file,
    std::string response_body) {
  // `ProcessURLFetchResults` may be called with a non-null `response_head` even
  // if the error code was not successful. Explicitly check for success here.
  ApiErrorCode error = GetErrorCode();
  if (!IsSuccessfulErrorCode(error)) {
    RunCallbackOnPrematureFailure(error);
    OnProcessURLFetchResultsComplete();
    return;
  }

  blocking_task_runner()->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&ParsePerson, std::move(response_body)),
      base::BindOnce(&CreateContactRequest::OnDataParsed,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CreateContactRequest::RunCallbackOnPrematureFailure(ApiErrorCode error) {
  std::move(callback_).Run(base::unexpected(error));
}

void CreateContactRequest::OnDataParsed(std::optional<Person> person) {
  if (person.has_value()) {
    std::move(callback_).Run(std::move(*person));
  } else {
    std::move(callback_).Run(base::unexpected(ApiErrorCode::PARSE_ERROR));
  }
  OnProcessURLFetchResultsComplete();
}

}  // namespace google_apis::people
