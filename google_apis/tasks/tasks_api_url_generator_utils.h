// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_TASKS_TASKS_API_URL_GENERATOR_UTILS_H_
#define GOOGLE_APIS_TASKS_TASKS_API_URL_GENERATOR_UTILS_H_

#include <string>

class GURL;

namespace google_apis::tasks {

// Returns a URL to fetch all the authenticated user's task lists.
// https://developers.google.com/tasks/reference/rest/v1/tasklists/list
GURL GetListTaskListsUrl();

// Returns a URL to fetch all tasks in the specified task list.
// https://developers.google.com/tasks/reference/rest/v1/tasks/list
GURL GetListTasksUrl(const std::string& task_list_id);

}  // namespace google_apis::tasks

#endif  // GOOGLE_APIS_TASKS_TASKS_API_URL_GENERATOR_UTILS_H_
