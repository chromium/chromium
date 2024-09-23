// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_TASKS_TASKS_API_URL_GENERATOR_UTILS_H_
#define GOOGLE_APIS_TASKS_TASKS_API_URL_GENERATOR_UTILS_H_

#include <optional>
#include <string>

class GURL;

namespace google_apis::tasks {

// Returns a URL to fetch all the authenticated user's task lists.
// `max_results` - maximum number of task lists returned on one page.
//                 Adds `maxResults` query parameter if not `std::nullopt`.
// `page_token`  - token specifying the result page to return.
//                 Adds `pageToken` query parameter if not empty.
// https://developers.google.com/tasks/reference/rest/v1/tasklists/list
GURL GetListTaskListsUrl(std::optional<int> max_results,
                         const std::string& page_token);

// Returns a URL to fetch all tasks in the specified task list.
// `task_list_id`      - task list identifier.
// `include_completed` - flag indicating whether completed tasks are returned
//                       in the result.
// `include_assigned`  - flag indicating whether assigned/shared tasks are
//                       returned in the result.
// `max_results`       - maximum number of tasks returned on one page. Adds
//                       `maxResults` query parameter if not `std::nullopt`.
// `page_token`        - token specifying the result page to return. Adds
//                       `pageToken` query parameter if not empty.
// https://developers.google.com/tasks/reference/rest/v1/tasks/list
GURL GetListTasksUrl(const std::string& task_list_id,
                     bool include_completed,
                     bool include_assigned,
                     std::optional<int> max_results,
                     const std::string& page_token);

// Returns a URL to partially update the specified task.
// `task_list_id` - task list identifier.
// `task_id`      - task identifier.
GURL GetPatchTaskUrl(const std::string& task_list_id,
                     const std::string& task_id);

// Returns a URL to create a new task on the specified task list.
// `task_list_id`     - task list identifier.
// `previous_task_id` - previous sibling task identifier.
GURL GetInsertTaskUrl(const std::string& task_list_id,
                      const std::string& previous_task_id);

}  // namespace google_apis::tasks

#endif  // GOOGLE_APIS_TASKS_TASKS_API_URL_GENERATOR_UTILS_H_
