// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SQL_EVICTION_CANDIDATE_AGGREGATOR_H_
#define NET_DISK_CACHE_SQL_EVICTION_CANDIDATE_AGGREGATOR_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "net/base/net_export.h"
#include "net/disk_cache/sql/sql_persistent_store.h"

namespace disk_cache {

// `EvictionCandidateAggregator` is a thread-safe class responsible for
// collecting eviction candidates from multiple shards, aggregating them, and
// then selecting which entries to evict based on the least recently used
// policy.
class NET_EXPORT_PRIVATE EvictionCandidateAggregator
    : public base::RefCountedThreadSafe<EvictionCandidateAggregator> {
 public:
  struct NET_EXPORT_PRIVATE EvictionCandidate {
    EvictionCandidate(SqlPersistentStore::ResId res_id,
                      SqlPersistentStore::ShardId shard_id,
                      int64_t bytes_usage,
                      base::Time last_used);
    ~EvictionCandidate();
    EvictionCandidate(EvictionCandidate&&);
    EvictionCandidate& operator=(EvictionCandidate&&);

    SqlPersistentStore::ResId res_id;
    SqlPersistentStore::ShardId shard_id;
    int64_t bytes_usage;
    base::Time last_used;
  };

  using EvictionCandidateList = std::vector<EvictionCandidate>;
  using EvictionCandidateSelectedCallback =
      base::OnceCallback<void(std::vector<SqlPersistentStore::ResId>,
                              int64_t bytes_usage,
                              base::TimeTicks post_task_time)>;

  explicit EvictionCandidateAggregator(
      int64_t size_to_be_removed,
      std::vector<scoped_refptr<base::SequencedTaskRunner>> task_runners);

  // Called by each shard to provide its list of eviction candidates on the task
  // runner assigned to the shard.
  // Once all shards have reported, this aggregates the candidates, selects
  // entries for eviction, and invokes `selected_callback` for each shard on its
  // corresponding SequencedTaskRunner from the `task_runners` vector passed to
  // the constructor. The callback for the *last* reporting shard is run
  // synchronously within this call, while others are posted as tasks.
  void OnCandidate(SqlPersistentStore::ShardId shard_id,
                   EvictionCandidateList candidate,
                   EvictionCandidateSelectedCallback selected_callback);

 private:
  friend class base::RefCountedThreadSafe<EvictionCandidateAggregator>;
  ~EvictionCandidateAggregator();

  // Safely adds a shard's candidates to the aggregation. Once all shards have
  // reported, returns true and the collected candidates and callbacks are moved
  // into `candidates_per_shard_out` and `selected_callbacks_out`.
  bool AddCandidates(
      SqlPersistentStore::ShardId shard_id,
      EvictionCandidateList new_candidates,
      EvictionCandidateSelectedCallback new_selected_callback,
      std::vector<EvictionCandidateList>& candidates_per_shard_out,
      std::vector<EvictionCandidateSelectedCallback>& selected_callbacks_out);

  // Aggregates candidates from all shards, sorts them by last-used time,
  // selects entries for eviction, and then posts tasks back to each shard with
  // the list of entries to be deleted.
  void AggregateCandidatesAndRunCallbacks(
      std::vector<EvictionCandidateList> candidates_per_shard,
      std::vector<EvictionCandidateSelectedCallback> selected_callbacks,
      SqlPersistentStore::ShardId last_shard_id);

  size_t GetSizeOfShards() const;

  // The total size of entries to be removed.
  const int64_t size_to_be_removed_;
  // The task runners for each shard, used to post back the eviction tasks.
  const std::vector<scoped_refptr<base::SequencedTaskRunner>> task_runners_;

  // Protects access to `candidates_per_shard_` and `selected_callbacks_`.
  base::Lock lock_;

  // A list of eviction candidates from each shard. This is not ordered by
  // ShardID, but in the order that OnCandidate was called.
  std::vector<EvictionCandidateList> candidates_per_shard_ GUARDED_BY(lock_);

  // Callbacks to run on each shard after the eviction candidates have been
  // selected. This is ordered by ShardId.
  std::vector<EvictionCandidateSelectedCallback> selected_callbacks_
      GUARDED_BY(lock_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SQL_EVICTION_CANDIDATE_AGGREGATOR_H_
