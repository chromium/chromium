// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_CLASSROOM_CLASSROOM_API_LIST_COURSE_WORK_MATERIALS_REQUEST_H_
#define GOOGLE_APIS_CLASSROOM_CLASSROOM_API_LIST_COURSE_WORK_MATERIALS_REQUEST_H_

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

class CourseWorkMaterial;

// Returns a list of course work materials that the requester is permitted to
// view.
// `course_id`  - identifier of the course.
// `page_token` - token specifying the result page to return.
//                Use an empty string to fetch the first page.
// `callback`   - done callback.
// https://developers.google.com/classroom/reference/rest/v1/courses.courseWorkMaterials/list
class ListCourseWorkMaterialRequest : public UrlFetchRequestBase {
 public:
  using Callback = base::OnceCallback<void(
      base::expected<std::unique_ptr<CourseWorkMaterial>, ApiErrorCode>
          result)>;

  ListCourseWorkMaterialRequest(RequestSender* sender,
                                const std::string& course_id,
                                const std::string& page_token,
                                Callback callback);
  ListCourseWorkMaterialRequest(const ListCourseWorkMaterialRequest&) = delete;
  ListCourseWorkMaterialRequest& operator=(
      const ListCourseWorkMaterialRequest&) = delete;
  ~ListCourseWorkMaterialRequest() override;

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
  void OnDataParsed(std::unique_ptr<CourseWorkMaterial> course_work_material);

  const std::string course_id_;
  const std::string page_token_;
  Callback callback_;

  base::WeakPtrFactory<ListCourseWorkMaterialRequest> weak_ptr_factory_{this};
};

}  // namespace classroom
}  // namespace google_apis

#endif  // GOOGLE_APIS_CLASSROOM_CLASSROOM_API_LIST_COURSE_WORK_MATERIALS_REQUEST_H_
