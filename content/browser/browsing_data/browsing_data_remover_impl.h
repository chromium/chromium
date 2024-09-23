// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVER_IMPL_H_
#define CONTENT_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVER_IMPL_H_

#include <stdint.h>

#include <map>
#include <optional>
#include <set>

#include "base/cancelable_callback.h"
#include "base/containers/queue.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/supports_user_data.h"
#include "base/synchronization/waitable_event_watcher.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/storage_partition_config.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/origin.h"

namespace content {

class BrowserContext;
class BrowsingDataFilterBuilder;
class StoragePartition;

class CONTENT_EXPORT BrowsingDataRemoverImpl
    : public BrowsingDataRemover,
      public base::SupportsUserData::Data {
 public:
  explicit BrowsingDataRemoverImpl(BrowserContext* browser_context);

  BrowsingDataRemoverImpl(const BrowsingDataRemoverImpl&) = delete;
  BrowsingDataRemoverImpl& operator=(const BrowsingDataRemoverImpl&) = delete;

  ~BrowsingDataRemoverImpl() override;

  // Is the BrowsingDataRemoverImpl currently in the process of removing data?
  bool IsRemovingForTesting() { return is_removing_; }

  // Removes storage buckets of a storage key.
  // If |storage_partition_config| is null, the operation will take place
  // on the profile's default storage partition.
  void RemoveStorageBucketsAndReply(
      const std::optional<StoragePartitionConfig> storage_partition_config,
      const blink::StorageKey& storage_key,
      const std::set<std::string>& storage_buckets,
      base::OnceClosure callback);

  // BrowsingDataRemover implementation:
  void SetEmbedderDelegate(
      BrowsingDataRemoverDelegate* embedder_delegate) override;
  bool DoesOriginMatchMaskForTesting(
      uint64_t origin_type_mask,
      const url::Origin& origin,
      storage::SpecialStoragePolicy* special_storage_policy) override;
  void Remove(const base::Time& delete_begin,
              const base::Time& delete_end,
              uint64_t remove_mask,
              uint64_t origin_type_mask) override;
  void RemoveWithFilter(
      const base::Time& delete_begin,
      const base::Time& delete_end,
      uint64_t remove_mask,
      uint64_t origin_type_mask,
      std::unique_ptr<BrowsingDataFilterBuilder> filter_builder) override;
  void RemoveAndReply(const base::Time& delete_begin,
                      const base::Time& delete_end,
                      uint64_t remove_mask,
                      uint64_t origin_type_mask,
                      Observer* observer) override;
  void RemoveWithFilterAndReply(
      const base::Time& delete_begin,
      const base::Time& delete_end,
      uint64_t remove_mask,
      uint64_t origin_type_mask,
      std::unique_ptr<BrowsingDataFilterBuilder> filter_builder,
      Observer* observer) override;

  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  void SetWouldCompleteCallbackForTesting(
      const base::RepeatingCallback<
          void(base::OnceClosure continue_to_completion)>& callback) override;

  const base::Time& GetLastUsedBeginTimeForTesting() override;
  uint64_t GetLastUsedRemovalMaskForTesting() override;
  uint64_t GetLastUsedOriginTypeMaskForTesting() override;
  std::optional<StoragePartitionConfig>
  GetLastUsedStoragePartitionConfigForTesting() override;
  uint64_t GetPendingTaskCountForTesting() override;

  void ClearClientHintCacheAndReply(const url::Origin& origin,
                                    base::OnceClosure callback);

  // Used for testing.
  void OverrideStoragePartitionForTesting(
      const StoragePartitionConfig& storage_partition_config,
      StoragePartition* storage_partition);

 protected:
  // A common reduction of all public Remove[WithFilter][AndReply] methods.
  virtual void RemoveInternal(
      const base::Time& delete_begin,
      const base::Time& delete_end,
      uint64_t remove_mask,
      uint64_t origin_type_mask,
      std::unique_ptr<BrowsingDataFilterBuilder> filter_builder,
      Observer* observer);

 private:
  // Testing the private RemovalTask.
  FRIEND_TEST_ALL_PREFIXES(BrowsingDataRemoverImplTest, MultipleTasks);
  FRIEND_TEST_ALL_PREFIXES(BrowsingDataRemoverImplTest, MultipleIdenticalTasks);

  // For debugging purposes. Please add new deletion tasks at the end.
  // This enum is recorded in a histogram, so don't change or reuse ids.
  // LINT.IfChange(TracingDataType)
  enum class TracingDataType {
    kSynchronous = 1,
    kEmbedderData = 2,
    kStoragePartition = 3,
    kHttpCache = 4,
    kHttpAndMediaCaches = 5,
    kReportingCache = 6,
    kChannelIds = 7,
    kNetworkHistory = 8,
    kAuthCache = 9,
    kCodeCaches = 10,
    kNetworkErrorLogging = 11,
    kTrustTokens = 12,
    kConversions = 13,
    kDeferredCookies = 14,
    kSharedStorage = 15,
    kPreflightCache = 16,
    kSharedDictionary = 17,
    kMaxValue = kSharedDictionary,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/history/enums.xml:BrowsingDataRemoverTasks)

  // Returns the suffix for the History.ClearBrowsingData.Duration.Task.{Task}
  // histogram
  const char* GetHistogramSuffix(TracingDataType task);

  // Represents a single removal task. Contains all parameters needed to execute
  // it and a pointer to the observer that added it. CONTENT_EXPORTed to be
  // visible in tests.
  struct CONTENT_EXPORT RemovalTask {
    RemovalTask(const base::Time& delete_begin,
                const base::Time& delete_end,
                uint64_t remove_mask,
                uint64_t origin_type_mask,
                std::unique_ptr<BrowsingDataFilterBuilder> filter_builder,
                Observer* observer);
    RemovalTask(RemovalTask&& other) noexcept;
    ~RemovalTask();

    // Returns true if the deletion parameters are equal.
    // Does not compare |observer| and |task_started|.
    bool IsSameDeletion(const RemovalTask& other);

    base::Time delete_begin;
    base::Time delete_end;
    uint64_t remove_mask;
    uint64_t origin_type_mask;
    std::unique_ptr<BrowsingDataFilterBuilder> filter_builder;
    std::vector<raw_ptr<Observer, VectorExperimental>> observers;
    base::TimeTicks task_started;
  };

  // Setter for |is_removing_|; DCHECKs that we can only start removing if we're
  // not already removing, and vice-versa.
  void SetRemoving(bool is_removing);

  // Executes the next removal task. Called after the previous task was finished
  // or directly from Remove() if the task queue was empty.
  void RunNextTask();

  // Removes the specified items related to browsing for a specific host. If the
  // provided |remove_url| is empty, data is removed for all origins; otherwise,
  // it is restricted by the origin filter origin (where implemented yet). The
  // |origin_type_mask| parameter defines the set of origins from which data
  // should be removed (protected, unprotected, or both).
  // TODO(ttr314): Remove "(where implemented yet)" constraint above once
  // crbug.com/113621 is done.
  // TODO(crbug.com/40458377): Support all backends w/ origin filter.
  void RemoveImpl(const base::Time& delete_begin,
                  const base::Time& delete_end,
                  uint64_t remove_mask,
                  BrowsingDataFilterBuilder* filter_builder,
                  uint64_t origin_type_mask);

  void OnDelegateDone(base::OnceClosure completion_closure,
                      uint64_t failed_data_types);

  // Notifies observers and transitions to the idle state.
  void Notify();

  // Called by the closures returned by CreateTaskCompletionClosure().
  // Checks if all tasks have completed, and if so, calls Notify().
  void OnTaskComplete(TracingDataType data_type, base::TimeTicks started);

  // Called when the storage buckets data has been removed.
  void DidRemoveStorageBuckets(base::OnceClosure callback);

  // Increments the number of pending tasks by one, and returns a OnceClosure
  // that calls OnTaskComplete(). The Remover is complete once all the closures
  // created by this method have been invoked.
  base::OnceClosure CreateTaskCompletionClosure(TracingDataType data_type);

  // Same as CreateTaskCompletionClosure() but guarantees that
  // OnTaskComplete() is called if the task is dropped. That can typically
  // happen when the connection is closed while an interface call is made.
  base::OnceClosure CreateTaskCompletionClosureForMojo(
      TracingDataType data_type);

  // Records unfinished tasks from |pending_sub_tasks_| after a delay.
  void RecordUnfinishedSubTasks();

  StoragePartition* GetStoragePartition(
      std::optional<StoragePartitionConfig> storage_partition_config);

  // This does the actual clearing of the client hint cache for the provided
  // origin. It should be invoked only via ClearClientHintCacheAndReply.
  void ClearClientHintCacheAndReplyImpl(const url::Origin& origin,
                                        base::OnceClosure callback);

  // Like GetWeakPtr(), but returns a weak pointer to BrowsingDataRemoverImpl
  // for internal purposes.
  base::WeakPtr<BrowsingDataRemoverImpl> GetWeakPtr();

  // The browser context we're to remove from.
  raw_ptr<BrowserContext> browser_context_;

  // A delegate to delete the embedder-specific data. Owned by the embedder.
  raw_ptr<BrowsingDataRemoverDelegate, DanglingUntriaged> embedder_delegate_;

  // Start time to delete from.
  base::Time delete_begin_;

  // End time to delete to.
  base::Time delete_end_;

  // The removal mask for the current removal operation.
  uint64_t remove_mask_ = 0;

  // From which types of origins should we remove data?
  uint64_t origin_type_mask_ = 0;

  // The StoragePartition from which data should be removed, or the default
  // if absent.
  std::optional<StoragePartitionConfig> storage_partition_config_ =
      std::nullopt;

  std::vector<std::string> domains_for_deferred_cookie_deletion_;

  // True if Remove has been invoked.
  bool is_removing_;

  // Removal tasks to be processed.
  std::deque<RemovalTask> task_queue_;

  // If non-null, the |would_complete_callback_| is called each time an instance
  // is about to complete a browsing data removal process, and has the ability
  // to artificially delay completion. Used for testing.
  base::RepeatingCallback<void(base::OnceClosure continue_to_completion)>
      would_complete_callback_;

  // Records which tasks of a deletion are currently active.
  std::set<TracingDataType> pending_sub_tasks_;

  uint64_t failed_data_types_ = 0;

  // Fires after some time to track slow tasks. Cancelled when all tasks
  // are finished.
  base::CancelableOnceClosure slow_pending_tasks_closure_;

  // Observers of the global state and individual tasks.
  base::ObserverList<Observer, true>::Unchecked observer_list_;

  // We do not own the StoragePartitions.
  std::map<StoragePartitionConfig, raw_ptr<StoragePartition>>
      storage_partitions_for_testing_;

  base::WeakPtrFactory<BrowsingDataRemoverImpl> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVER_IMPL_H_
