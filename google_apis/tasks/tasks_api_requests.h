// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_TASKS_TASKS_API_REQUESTS_H_
#define GOOGLE_APIS_TASKS_TASKS_API_REQUESTS_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "google_apis/common/base_requests.h"
#include "google_apis/tasks/tasks_api_request_types.h"
#include "google_apis/tasks/tasks_api_response_types.h"

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

namespace tasks {

enum class TaskStatus;

// Fetches all the authenticated user's task lists and invokes `callback_` when
// done.
// `page_token` - token specifying the result page to return. Optional.
// https://developers.google.com/tasks/reference/rest/v1/tasklists/list
class ListTaskListsRequest : public UrlFetchRequestBase {
 public:
  using Callback = base::OnceCallback<void(
      base::expected<std::unique_ptr<TaskLists>, ApiErrorCode>)>;

  ListTaskListsRequest(RequestSender* sender,
                       const std::string& page_token,
                       Callback callback);
  ListTaskListsRequest(const ListTaskListsRequest&) = delete;
  ListTaskListsRequest& operator=(const ListTaskListsRequest&) = delete;
  ~ListTaskListsRequest() override;

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
  static std::unique_ptr<TaskLists> Parse(std::string json);
  void OnDataParsed(std::unique_ptr<TaskLists> task_lists);

  const std::string page_token_;
  Callback callback_;

  base::WeakPtrFactory<ListTaskListsRequest> weak_ptr_factory_{this};
};

// Fetches all tasks in the specified task list (`task_list_id`) and invokes
// `callback_` when done.
// `page_token`       - token specifying the result page to return. Optional.
// `include_assigned` - requests assigned/shared tasks.
// https://developers.google.com/tasks/reference/rest/v1/tasks/list
class ListTasksRequest : public UrlFetchRequestBase {
 public:
  using Callback = base::OnceCallback<void(
      base::expected<std::unique_ptr<Tasks>, ApiErrorCode>)>;

  ListTasksRequest(RequestSender* sender,
                   const std::string& task_list_id,
                   const std::string& page_token,
                   bool include_assigned,
                   Callback callback);
  ListTasksRequest(const ListTasksRequest&) = delete;
  ListTasksRequest& operator=(const ListTasksRequest&) = delete;
  ~ListTasksRequest() override;

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
  static std::unique_ptr<Tasks> Parse(std::string json);
  void OnDataParsed(std::unique_ptr<Tasks> task_lists);

  const std::string task_list_id_;
  const std::string page_token_;
  const bool include_assigned_;
  Callback callback_;

  base::WeakPtrFactory<ListTasksRequest> weak_ptr_factory_{this};
};

// Partially updates the specified task.
// `payload` - the request body with the fields to update.
// https://developers.google.com/tasks/reference/rest/v1/tasks/patch
class PatchTaskRequest : public UrlFetchRequestBase {
 public:
  using Callback = base::OnceCallback<void(
      base::expected<std::unique_ptr<Task>, ApiErrorCode>)>;

  PatchTaskRequest(RequestSender* sender,
                   const std::string& task_list_id,
                   const std::string& task_id,
                   const TaskRequestPayload& payload,
                   Callback callback);
  PatchTaskRequest(const PatchTaskRequest&) = delete;
  PatchTaskRequest& operator=(const PatchTaskRequest&) = delete;
  ~PatchTaskRequest() override;

 protected:
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
  void RunCallbackOnPrematureFailure(ApiErrorCode code) override;

 private:
  static std::unique_ptr<Task> Parse(std::string json);
  void OnDataParsed(std::unique_ptr<Task> task_lists);

  const std::string task_list_id_;
  const std::string task_id_;
  const TaskRequestPayload payload_;
  Callback callback_;

  base::WeakPtrFactory<PatchTaskRequest> weak_ptr_factory_{this};
};

// Creates a new task on the specified task list.
// `task_list_id`     - task list identifier. Required.
// `previous_task_id` - previous sibling task identifier. If the task is created
//                      at the first position among its siblings, this parameter
//                      is omitted. Optional.
// `payload`          - the request body with the fields needed to create a new
//                      task.
// `callback`         - done callback.
// https://developers.google.com/tasks/reference/rest/v1/tasks/insert
class InsertTaskRequest : public UrlFetchRequestBase {
 public:
  using Callback = base::OnceCallback<void(
      base::expected<std::unique_ptr<Task>, ApiErrorCode>)>;

  InsertTaskRequest(RequestSender* sender,
                    const std::string& task_list_id,
                    const std::string& previous_task_id,
                    const TaskRequestPayload& payload,
                    Callback callback);
  InsertTaskRequest(const InsertTaskRequest&) = delete;
  InsertTaskRequest& operator=(const InsertTaskRequest&) = delete;
  ~InsertTaskRequest() override;

 protected:
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
  void RunCallbackOnPrematureFailure(ApiErrorCode code) override;

 private:
  static std::unique_ptr<Task> Parse(std::string json);
  void OnDataParsed(std::unique_ptr<Task> task_lists);

  const std::string task_list_id_;
  const std::string previous_task_id_;
  const TaskRequestPayload payload_;
  Callback callback_;

  base::WeakPtrFactory<InsertTaskRequest> weak_ptr_factory_{this};
};

}  // namespace tasks
}  // namespace google_apis

#endif  // GOOGLE_APIS_TASKS_TASKS_API_REQUESTS_H_
