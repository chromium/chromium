// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_FEEDBACK_PRIVATE_LOG_SOURCE_ACCESS_MANAGER_H_
#define EXTENSIONS_BROWSER_API_FEEDBACK_PRIVATE_LOG_SOURCE_ACCESS_MANAGER_H_

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "components/feedback/redaction_tool/redaction_tool.h"
#include "components/feedback/system_logs/system_logs_source.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/api/feedback_private/access_rate_limiter.h"
#include "extensions/common/api/feedback_private.h"
#include "extensions/common/extension_id.h"

namespace extensions {

// Provides bookkeepping for SingleLogSource usage. It ensures that:
// - Each extension can have only one SingleLogSource for a particular source.
// - A source may not be accessed too frequently by an extension.
class LogSourceAccessManager {
 public:
  using ReadLogSourceCallback = base::OnceCallback<void(
      std::unique_ptr<api::feedback_private::ReadLogSourceResult>)>;

  explicit LogSourceAccessManager(content::BrowserContext* context);

  LogSourceAccessManager(const LogSourceAccessManager&) = delete;
  LogSourceAccessManager& operator=(const LogSourceAccessManager&) = delete;

  ~LogSourceAccessManager();

  // Call this to override the maximum burst access count of the rate limiter.
  static void SetMaxNumBurstAccessesForTesting(int num_accesses);

  // To override the default rate-limiting mechanism of this function, pass in
  // a TimeDelta representing the desired minimum time between consecutive reads
  // of a source from an extension. Does not take ownership of |timeout|. When
  // done testing, call this function again with |timeout|=nullptr to reset to
  // the default behavior.
  static void SetRateLimitingTimeoutForTesting(const base::TimeDelta* timeout);

  // Override the default base::Time clock with a custom clock for testing.
  void SetTickClockForTesting(const base::TickClock* clock) {
    tick_clock_ = clock;
  }

  // Initiates a fetch from a log source, as specified in |params|. See
  // feedback_private.idl for more info about the actual parameters.
  bool FetchFromSource(const api::feedback_private::ReadLogSourceParams& params,
                       const ExtensionId& extension_id,
                       ReadLogSourceCallback callback);

  // Each log source may not have more than this number of readers accessing it,
  // regardless of extension.
  static constexpr int kMaxReadersPerSource = 10;

 private:
  FRIEND_TEST_ALL_PREFIXES(LogSourceAccessManagerTest,
                           MaxNumberOfOpenLogSourcesSameExtension);
  FRIEND_TEST_ALL_PREFIXES(LogSourceAccessManagerTest,
                           MaxNumberOfOpenLogSourcesDifferentExtensions);

  // Contains a source/extension pair.
  struct SourceAndExtension {
    explicit SourceAndExtension(api::feedback_private::LogSource source,
                                const ExtensionId& extension_id);

    bool operator<(const SourceAndExtension& other) const {
      return std::make_pair(source, extension_id) <
             std::make_pair(other.source, other.extension_id);
    }

    // The log source that this handle is accessing.
    api::feedback_private::LogSource source;
    // ID of the extension that opened this handle.
    ExtensionId extension_id;
  };

  using ResourceId = int;

  // Returned when there was an error creating a new resource or looking for an
  // existing resource.
  static constexpr ResourceId kInvalidResourceId = 0;

  // Creates a new LogSourceResource for the source and extension indicated by
  // |source| and |extension_id|. Stores the new resource in the API Resource
  // Manager, and uses the resource ID as a key for a new entry in
  // |open_handles_|, with value being a SourceAndExtension containing |source|
  // and |extension_id|.
  //
  // Returns the nonzero ID of the newly created LogSourceResource, or
  // |kInvalidResourceId| if a new resource could not be created.
  ResourceId CreateResource(api::feedback_private::LogSource source,
                            const ExtensionId& extension_id);

  // Callback that is passed to the log source from FetchFromSource.
  // Arguments:
  // - extension_id: ID of extension that opened the log source.
  // - resource_id: Resource ID provided by API Resource Manager for the reader.
  // - delete_source: Set this if the source opened by |handle| should be
  //   removed from both the API Resource Manager and from |open_handles_|.
  // - callback: Callback for sending the response as a ReadLogSourceResult
  //   struct.
  // - response: Contains the result from an operation to fetch from system
  //   log(s).
  void OnFetchComplete(
      const ExtensionId& extension_id,
      ResourceId resource_id,
      bool delete_source,
      ReadLogSourceCallback callback,
      std::unique_ptr<system_logs::SystemLogsResponse> response);

  // Removes an existing log source handle indicated by |id| from
  // |open_handles_|.
  void RemoveHandle(ResourceId id);

  // Returns the number of entries in |open_handles_| with source=|source|.
  size_t GetNumActiveResourcesForSource(
      api::feedback_private::LogSource source) const;

  // Attempts to update the |last_access_time| field for the SourceAndExtension
  // |open_handles_[id]|, to record that the source is being accessed by the
  // handle right now. If less than |min_time_between_reads_| has elapsed since
  // the last successful read, does not update |last_access_times|, and instead
  // returns false. Otherwise returns true.
  bool UpdateSourceAccessTime(ResourceId id);

  // Keeps track of the last time each source was accessed by each extension.
  // Each time FetchFromSource() is called, the timestamp gets updated.
  //
  // This intentionally kept separate from |sources_| because entries can be
  // removed from and re-added to |sources_|, but that should not erase the
  // recorded access times.
  std::map<SourceAndExtension, std::unique_ptr<AccessRateLimiter>>
      rate_limiters_;

  // Contains all open handles, each uniquely identified by a ResourceId and
  // additionally described by a SourceAndExtension struct.
  std::map<ResourceId, std::unique_ptr<SourceAndExtension>> open_handles_;

  // Keep count of the number of reader handles (resources) for each source.
  std::map<api::feedback_private::LogSource, size_t> num_readers_per_source_;

  // For fetching browser resources like ApiResourceManager.
  raw_ptr<content::BrowserContext> context_;

  // Provides a timer clock implementation for keeping track of access times.
  // Can override the default clock for testing.
  raw_ptr<const base::TickClock> tick_clock_;

  // For removing PII from log strings from log sources.
  scoped_refptr<base::SequencedTaskRunner> task_runner_for_redactor_;
  scoped_refptr<redaction::RedactionToolContainer> redactor_container_;

  base::WeakPtrFactory<LogSourceAccessManager> weak_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_FEEDBACK_PRIVATE_LOG_SOURCE_ACCESS_MANAGER_H_
