// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_CLASSROOM_CLASSROOM_API_LIST_COURSES_REQUEST_H_
#define GOOGLE_APIS_CLASSROOM_CLASSROOM_API_LIST_COURSES_REQUEST_H_

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

class Courses;

// Returns a list of courses that the requesting user is permitted to view,
// restricted to those that match the request.
// `student_id` - restricts returned courses to those having a student with
//                the specified identifier. Use an empty string to avoid
//                filtering by student id.
// `teacher_id` - restricts returned courses to those having a teacher with
//                the specified identifier. Use an empty string to avoid
//                filtering by teacher id.
// `page_token` - token specifying the result page to return.
//                Use an empty string to fetch the first page.
// `callback`   - done callback.
// https://developers.google.com/classroom/reference/rest/v1/courses/list
class ListCoursesRequest : public UrlFetchRequestBase {
 public:
  using Callback = base::OnceCallback<void(
      base::expected<std::unique_ptr<Courses>, ApiErrorCode> result)>;

  ListCoursesRequest(RequestSender* sender,
                     const std::string& student_id,
                     const std::string& teacher_id,
                     const std::string& page_token,
                     Callback callback);
  ListCoursesRequest(const ListCoursesRequest&) = delete;
  ListCoursesRequest& operator=(const ListCoursesRequest&) = delete;
  ~ListCoursesRequest() override;

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
  void OnDataParsed(std::unique_ptr<Courses> courses);

  const std::string student_id_;
  const std::string teacher_id_;
  const std::string page_token_;
  Callback callback_;

  base::WeakPtrFactory<ListCoursesRequest> weak_ptr_factory_{this};
};

}  // namespace classroom
}  // namespace google_apis

#endif  // GOOGLE_APIS_CLASSROOM_CLASSROOM_API_LIST_COURSES_REQUEST_H_
