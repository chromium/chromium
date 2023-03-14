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

class TaskLists;
class Tasks;

// Fetches all the authenticated user's task lists and invokes `callback_` when
// done.
// https://developers.google.com/tasks/reference/rest/v1/tasklists/list
class ListTaskListsRequest : public UrlFetchRequestBase {
 public:
  using Callback = base::OnceCallback<void(
      base::expected<std::unique_ptr<TaskLists>, ApiErrorCode>)>;

  ListTaskListsRequest(RequestSender* sender, Callback callback);
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

  Callback callback_;

  base::WeakPtrFactory<ListTaskListsRequest> weak_ptr_factory_{this};
};

// Fetches all tasks in the specified task list (`task_list_id`) and invokes
// `callback_` when done.
// https://developers.google.com/tasks/reference/rest/v1/tasks/list
class ListTasksRequest : public UrlFetchRequestBase {
 public:
  using Callback = base::OnceCallback<void(
      base::expected<std::unique_ptr<Tasks>, ApiErrorCode>)>;

  ListTasksRequest(RequestSender* sender,
                   Callback callback,
                   const std::string& task_list_id);
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

  Callback callback_;
  std::string task_list_id_;

  base::WeakPtrFactory<ListTasksRequest> weak_ptr_factory_{this};
};

}  // namespace tasks
}  // namespace google_apis

#endif  // GOOGLE_APIS_TASKS_TASKS_API_REQUESTS_H_
