// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_PEOPLE_PEOPLE_API_REQUESTS_H_
#define GOOGLE_APIS_PEOPLE_PEOPLE_API_REQUESTS_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/base_requests.h"
#include "google_apis/people/people_api_request_types.h"

namespace google_apis::people {

struct Person;

// Requires `GaiaConstants::kContactsOAuth2Scope`.
//
// From the People API reference:
//
// Create a new contact and return the person resource for that contact.
//
// Mutate requests for the same user should be sent sequentially to avoid
// increased latency and failures.
//
// https://developers.google.com/people/api/rest/v1/people/createContact
class CreateContactRequest : public UrlFetchRequestBase {
 public:
  using Callback =
      base::OnceCallback<void(base::expected<Person, ApiErrorCode>)>;

  CreateContactRequest(RequestSender* sender,
                       Contact payload,
                       Callback callback);
  CreateContactRequest(const CreateContactRequest&) = delete;
  CreateContactRequest& operator=(const CreateContactRequest&) = delete;
  ~CreateContactRequest() override;

 private:
  // UrlFetchRequestBase:
  GURL GetURL() const override;
  ApiErrorCode MapReasonToError(ApiErrorCode code,
                                const std::string& reason) override;
  bool IsSuccessfulErrorCode(ApiErrorCode error) override;
  HttpRequestMethod GetRequestType() const override;
  bool GetContentData(std::string* upload_content_type,
                      std::string* upload_content) override;
  void ProcessURLFetchResults(
      const network::mojom::URLResponseHead* response_head,
      const base::FilePath response_file,
      std::string response_body) override;
  void RunCallbackOnPrematureFailure(ApiErrorCode error) override;

  void OnDataParsed(std::optional<Person> person);

  base::Value::Dict contact_payload_;
  Callback callback_;

  base::WeakPtrFactory<CreateContactRequest> weak_ptr_factory_{this};
};

}  // namespace google_apis::people

#endif  // GOOGLE_APIS_PEOPLE_PEOPLE_API_REQUESTS_H_
