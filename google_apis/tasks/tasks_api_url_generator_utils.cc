// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/tasks/tasks_api_url_generator_utils.h"

#include <optional>
#include <string>

#include "base/check.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace google_apis::tasks {
namespace {

constexpr char kFieldsParameterName[] = "fields";
constexpr char kMaxResultsParameterName[] = "maxResults";
constexpr char kPageTokenParameterName[] = "pageToken";
constexpr char kPreviousTaskParameterName[] = "previous";
constexpr char kShowAssignedParameterName[] = "showAssigned";
constexpr char kShowCompletedParameterName[] = "showCompleted";

constexpr char kTaskListsListUrl[] = "tasks/v1/users/@me/lists";
constexpr char kTaskListsListRequestedFields[] =
    "kind,items(id,title,updated),nextPageToken";

constexpr char kTasksListUrlTemplate[] = "tasks/v1/lists/$1/tasks";
constexpr char kTasksListRequestedFields[] =
    "kind,items(id,title,status,parent,position,due,links(type),notes,updated,"
    "webViewLink),nextPageToken";
constexpr char kTasksListRequestedFieldsWithAssignmentInfo[] =
    "kind,items(id,title,status,parent,position,due,links(type),notes,updated,"
    "webViewLink,assignmentInfo(surfaceType)),nextPageToken";

constexpr char kTaskUrlTemplate[] = "tasks/v1/lists/$1/tasks/$2";

GURL GetBaseUrl() {
  return GaiaUrls::GetInstance()->tasks_api_origin_url();
}

}  // namespace

GURL GetListTaskListsUrl(std::optional<int> max_results,
                         const std::string& page_token) {
  GURL url = GetBaseUrl().Resolve(kTaskListsListUrl);
  url = net::AppendOrReplaceQueryParameter(url, kFieldsParameterName,
                                           kTaskListsListRequestedFields);
  if (max_results.has_value()) {
    url = net::AppendOrReplaceQueryParameter(
        url, kMaxResultsParameterName,
        base::NumberToString(max_results.value()));
  }
  if (!page_token.empty()) {
    url = net::AppendOrReplaceQueryParameter(url, kPageTokenParameterName,
                                             page_token);
  }
  return url;
}

GURL GetListTasksUrl(const std::string& task_list_id,
                     bool include_completed,
                     bool include_assigned,
                     std::optional<int> max_results,
                     const std::string& page_token) {
  CHECK(!task_list_id.empty());
  GURL url = GetBaseUrl().Resolve(base::ReplaceStringPlaceholders(
      kTasksListUrlTemplate, {task_list_id}, nullptr));
  url = net::AppendOrReplaceQueryParameter(
      url, kFieldsParameterName,
      include_assigned ? kTasksListRequestedFieldsWithAssignmentInfo
                       : kTasksListRequestedFields);
  if (!include_completed) {
    // The default is `true`.
    url = net::AppendOrReplaceQueryParameter(url, kShowCompletedParameterName,
                                             "false");
  }
  if (include_assigned) {
    // The default is `false`.
    url = net::AppendOrReplaceQueryParameter(url, kShowAssignedParameterName,
                                             "true");
  }
  if (max_results.has_value()) {
    url = net::AppendOrReplaceQueryParameter(
        url, kMaxResultsParameterName,
        base::NumberToString(max_results.value()));
  }
  if (!page_token.empty()) {
    url = net::AppendOrReplaceQueryParameter(url, kPageTokenParameterName,
                                             page_token);
  }
  return url;
}

GURL GetPatchTaskUrl(const std::string& task_list_id,
                     const std::string& task_id) {
  CHECK(!task_list_id.empty());
  CHECK(!task_id.empty());
  return GetBaseUrl().Resolve(base::ReplaceStringPlaceholders(
      kTaskUrlTemplate, {task_list_id, task_id}, nullptr));
}

GURL GetInsertTaskUrl(const std::string& task_list_id,
                      const std::string& previous_task_id) {
  CHECK(!task_list_id.empty());
  GURL url = GetBaseUrl().Resolve(base::ReplaceStringPlaceholders(
      kTasksListUrlTemplate, {task_list_id}, nullptr));
  if (!previous_task_id.empty()) {
    url = net::AppendOrReplaceQueryParameter(url, kPreviousTaskParameterName,
                                             previous_task_id);
  }
  return url;
}

}  // namespace google_apis::tasks
