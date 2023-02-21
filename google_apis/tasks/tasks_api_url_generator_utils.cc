// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/tasks/tasks_api_url_generator_utils.h"

#include <string>

#include "base/check.h"
#include "base/strings/string_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace google_apis::tasks {
namespace {

constexpr char kFieldsParameterName[] = "fields";

constexpr char kTaskListsListUrl[] = "tasks/v1/users/@me/lists";
constexpr char kTaskListsListRequestedFields[] = "kind,items(id,title,updated)";

constexpr char kTasksListUrlTemplate[] = "tasks/v1/lists/$1/tasks";
constexpr char kTasksListRequestedFields[] =
    "kind,items(id,title,status,parent)";

GURL GetBaseUrl() {
  return GaiaUrls::GetInstance()->google_apis_origin_url();
}

}  // namespace

GURL GetListTaskListsUrl() {
  GURL url = GetBaseUrl().Resolve(kTaskListsListUrl);
  url = net::AppendOrReplaceQueryParameter(url, kFieldsParameterName,
                                           kTaskListsListRequestedFields);
  return url;
}

GURL GetListTasksUrl(const std::string& task_list_id) {
  DCHECK(!task_list_id.empty());
  GURL url = GetBaseUrl().Resolve(base::ReplaceStringPlaceholders(
      kTasksListUrlTemplate, {task_list_id}, nullptr));
  url = net::AppendOrReplaceQueryParameter(url, kFieldsParameterName,
                                           kTasksListRequestedFields);
  return url;
}

}  // namespace google_apis::tasks
