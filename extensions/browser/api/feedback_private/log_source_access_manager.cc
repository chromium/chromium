// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/feedback_private/log_source_access_manager.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/strings/string_split.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "base/time/default_tick_clock.h"
#include "extensions/browser/api/api_resource_manager.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/feedback_private/feedback_private_delegate.h"
#include "extensions/browser/api/feedback_private/log_source_resource.h"

namespace extensions {

namespace {

using LogSource = api::feedback_private::LogSource;
using ReadLogSourceParams = api::feedback_private::ReadLogSourceParams;
using ReadLogSourceResult = api::feedback_private::ReadLogSourceResult;
using SystemLogsResponse = system_logs::SystemLogsResponse;

// Default value of |g_max_num_burst_accesses|.
constexpr int kDefaultMaxNumBurstAccesses = 10;

// The minimum time between consecutive reads of a log source by a particular
// extension.
constexpr base::TimeDelta kDefaultRateLimitingTimeout =
    base::TimeDelta::FromMilliseconds(1000);

// The maximum number of accesses on a single log source that can be allowed
// before the next recharge increment. See access_rate_limiter.h for more info.
int g_max_num_burst_accesses = kDefaultMaxNumBurstAccesses;

// If this is null, then |kDefaultRateLimitingTimeoutMs| is used as the timeout.
const base::TimeDelta* g_rate_limiting_timeout = nullptr;

base::TimeDelta GetMinTimeBetweenReads() {
  return g_rate_limiting_timeout ? *g_rate_limiting_timeout
                                 : kDefaultRateLimitingTimeout;
}

// SystemLogsResponse is a map of strings -> strings. The map value has the
// actual log contents, a string containing all lines, separated by newlines.
// This function extracts the individual lines and converts them into a vector
// of strings, each string containing a single line.
void GetLogLinesFromSystemLogsResponse(const SystemLogsResponse& response,
                                       std::vector<std::string>* log_lines) {
  for (const std::pair<std::string, std::string>& pair : response) {
    std::vector<std::string> new_lines = base::SplitString(
        pair.second, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    log_lines->reserve(log_lines->size() + new_lines.size());
    log_lines->insert(log_lines->end(), new_lines.begin(), new_lines.end());
  }
}

// Anonymizes the strings in |result|.
void AnonymizeResults(
    scoped_refptr<feedback::AnonymizerToolContainer> anonymizer_container,
    ReadLogSourceResult* result) {
  feedback::AnonymizerTool* anonymizer = anonymizer_container->Get();
  for (std::string& line : result->log_lines)
    line = anonymizer->Anonymize(line);
}

}  // namespace

LogSourceAccessManager::LogSourceAccessManager(content::BrowserContext* context)
    : context_(context),
      tick_clock_(base::DefaultTickClock::GetInstance()),
      task_runner_for_anonymizer_(base::CreateSequencedTaskRunner(
          // User visible as the feedback_api is used by the Chrome (OS)
          // feedback extension while the user may be looking at a spinner.
          {base::ThreadPool(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})),
      anonymizer_container_(
          base::MakeRefCounted<feedback::AnonymizerToolContainer>(
              task_runner_for_anonymizer_,
              /* first_party_extension_ids= */ nullptr)) {}

LogSourceAccessManager::~LogSourceAccessManager() {}

// static
void LogSourceAccessManager::SetMaxNumBurstAccessesForTesting(
    int num_accesses) {
  g_max_num_burst_accesses = num_accesses;
}

// static
void LogSourceAccessManager::SetRateLimitingTimeoutForTesting(
    const base::TimeDelta* timeout) {
  g_rate_limiting_timeout = timeout;
}

bool LogSourceAccessManager::FetchFromSource(const ReadLogSourceParams& params,
                                             const std::string& extension_id,
                                             ReadLogSourceCallback callback) {
  int requested_resource_id =
      params.reader_id ? *params.reader_id : kInvalidResourceId;
  ApiResourceManager<LogSourceResource>* resource_manager =
      ApiResourceManager<LogSourceResource>::Get(context_);

  const ResourceId resource_id =
      requested_resource_id != kInvalidResourceId
          ? requested_resource_id
          : CreateResource(params.source, extension_id);
  if (resource_id == kInvalidResourceId)
    return false;

  LogSourceResource* resource =
      resource_manager->Get(extension_id, resource_id);
  if (!resource)
    return false;

  // Enforce the rules: rate-limit access to the source from the current reader
  // handle. If not enough time has elapsed since the last access, do not read
  // from the source, but instead return an empty response. From the caller's
  // perspective, there is no new data. There is no need for the caller to keep
  // track of the time since last access.
  if (!UpdateSourceAccessTime(resource_id)) {
    std::move(callback).Run(
        std::make_unique<api::feedback_private::ReadLogSourceResult>());
    return true;
  }

  // If the API call requested a non-incremental access, clean up the
  // SingleLogSource by removing its API resource. Even if the existing source
  // were originally created as incremental, passing in incremental=false on a
  // later access indicates that the source should be closed afterwards.
  const bool delete_resource_when_done = !params.incremental;

  resource->GetLogSource()->Fetch(
      base::BindOnce(&LogSourceAccessManager::OnFetchComplete,
                     weak_factory_.GetWeakPtr(), extension_id, resource_id,
                     delete_resource_when_done, std::move(callback)));
  return true;
}

void LogSourceAccessManager::OnFetchComplete(
    const std::string& extension_id,
    ResourceId resource_id,
    bool delete_resource,
    ReadLogSourceCallback callback,
    std::unique_ptr<SystemLogsResponse> response) {
  auto result = std::make_unique<ReadLogSourceResult>();

  // Always return invalid resource ID if there is a cleanup.
  result->reader_id = delete_resource ? kInvalidResourceId : resource_id;

  GetLogLinesFromSystemLogsResponse(*response, &result->log_lines);

  // Retrieve result pointer before the PostTaskAndReply to fix issues with
  // an undefined execution order of arguments in a function call
  // (std::move(result) being executed before result.get()).
  ReadLogSourceResult* result_ptr = result.get();
  task_runner_for_anonymizer_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(AnonymizeResults, anonymizer_container_,
                     base::Unretained(result_ptr)),
      base::BindOnce(std::move(callback), std::move(result)));

  if (delete_resource) {
    // This should also remove the entry from |sources_|.
    ApiResourceManager<LogSourceResource>::Get(context_)->Remove(extension_id,
                                                                 resource_id);
  }
}

void LogSourceAccessManager::RemoveHandle(ResourceId id) {
  auto iter = open_handles_.find(id);
  if (iter == open_handles_.end())
    return;
  const SourceAndExtension& source_and_extension = *iter->second;
  const LogSource source = source_and_extension.source;
  if (num_readers_per_source_.find(source) != num_readers_per_source_.end())
    num_readers_per_source_[source]--;

  open_handles_.erase(id);
}

LogSourceAccessManager::SourceAndExtension::SourceAndExtension(
    LogSource source,
    const std::string& extension_id)
    : source(source), extension_id(extension_id) {}

LogSourceAccessManager::ResourceId LogSourceAccessManager::CreateResource(
    LogSource source,
    const std::string& extension_id) {
  // Enforce the rules: Do not create too many SingleLogSource objects to read
  // from a source, even if they are from different extensions.
  if (GetNumActiveResourcesForSource(source) >= kMaxReadersPerSource)
    return kInvalidResourceId;

  auto new_resource = std::make_unique<LogSourceResource>(
      extension_id, ExtensionsAPIClient::Get()
                        ->GetFeedbackPrivateDelegate()
                        ->CreateSingleLogSource(source));

  auto* resource_manager = ApiResourceManager<LogSourceResource>::Get(context_);
  // Create an ID for the resource using the API Resource Manager, but don't
  // release ownership of it until a valid ID has been secured.
  ResourceId resource_id = resource_manager->Add(new_resource.get());
  if (resource_id == kInvalidResourceId)
    return kInvalidResourceId;

  if (open_handles_.find(resource_id) != open_handles_.end())
    return kInvalidResourceId;

  // Now that |resource_id| has been determined to be valid, release ownership
  // of the LogSourceResource, which is now owned by the API resource manager.
  new_resource.release();

  // The resource ID isn't known until |new_resource| is added to the API
  // Resource Manager, but it needs to be passed into the resource afterward, so
  // that the resource can unregister itself from LogSourceAccessManager. It is
  // passed in as part of a callback.
  resource_manager->Get(extension_id, resource_id)
      ->set_unregister_callback(
          base::Bind(&LogSourceAccessManager::RemoveHandle,
                     weak_factory_.GetWeakPtr(), resource_id));

  open_handles_.emplace(
      resource_id, std::make_unique<SourceAndExtension>(source, extension_id));
  num_readers_per_source_[source]++;

  return resource_id;
}

bool LogSourceAccessManager::UpdateSourceAccessTime(ResourceId id) {
  auto iter = open_handles_.find(id);
  if (iter == open_handles_.end())
    return false;

  const SourceAndExtension& key = *iter->second;
  if (rate_limiters_.find(key) == rate_limiters_.end()) {
    rate_limiters_.emplace(key, std::make_unique<AccessRateLimiter>(
                                    g_max_num_burst_accesses,
                                    GetMinTimeBetweenReads(), tick_clock_));
  }
  return rate_limiters_[key]->AttemptAccess();
}

size_t LogSourceAccessManager::GetNumActiveResourcesForSource(
    LogSource source) const {
  auto iter = num_readers_per_source_.find(source);
  if (iter == num_readers_per_source_.end())
    return 0;
  return iter->second;
}

}  // namespace extensions
