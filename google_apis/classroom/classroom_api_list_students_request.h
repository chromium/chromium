// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_CLASSROOM_CLASSROOM_API_LIST_STUDENTS_REQUEST_H_
#define GOOGLE_APIS_CLASSROOM_CLASSROOM_API_LIST_STUDENTS_REQUEST_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "google_apis/common/base_requests.h"

class GURL;

namespace base {
class FilePath;
}  // namespace base

namespace network::mojom {
class URLResponseHead;
}  // namespace network::mojom

namespace google_apis {

enum ApiErrorCode;
class RequestSender;

namespace classroom {

class Students;

// Returns a list of students in a course that the requesting user is permitted
// to view, restricted to those that match the request. `course_id`  - restricts
// returned students to those in a class with this id. `page_token` - token
// specifying the result page to return.
//                Use an empty string to fetch the first page.
// `callback`   - done callback.
// https://developers.google.com/classroom/reference/rest/v1/courses.students/list
class ListStudentsRequest : public UrlFetchRequestBase {
 public:
  using Callback = base::OnceCallback<void(
      base::expected<std::unique_ptr<Students>, ApiErrorCode> result)>;

  ListStudentsRequest(RequestSender* sender,
                      const std::string& course_id,
                      const std::string& page_token,
                      Callback callback);
  ListStudentsRequest(const ListStudentsRequest&) = delete;
  ListStudentsRequest& operator=(const ListStudentsRequest&) = delete;
  ~ListStudentsRequest() override;

 protected:
  // UrlFetchRequestBase:
  GURL GetURL() const override;
  ApiErrorCode MapReasonToError(ApiErrorCode code,
                                const std::string& reason) override;
  bool IsSuccessfulErrorCode(ApiErrorCode error) override;
  void ProcessURLFetchResults(
      const network::mojom::URLResponseHead* response_head,
      const base::FilePath response_file,
      std::string response_body) override;
  void RunCallbackOnPrematureFailure(ApiErrorCode code) override;

 private:
  void OnDataParsed(std::unique_ptr<Students> students);

  const std::string course_id_;
  const std::string page_token_;
  Callback callback_;

  base::WeakPtrFactory<ListStudentsRequest> weak_ptr_factory_{this};
};

}  // namespace classroom
}  // namespace google_apis

#endif  // GOOGLE_APIS_CLASSROOM_CLASSROOM_API_LIST_STUDENTS_REQUEST_H_
