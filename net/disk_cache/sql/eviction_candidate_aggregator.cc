// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/eviction_candidate_aggregator.h"

#include <algorithm>
#include <iterator>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"

namespace disk_cache {

EvictionCandidateAggregator::EvictionCandidate::EvictionCandidate(
    SqlPersistentStore::ResId res_id,
    SqlPersistentStore::ShardId shard_id,
    int64_t bytes_usage,
    base::Time last_used)
    : res_id(res_id),
      shard_id(shard_id),
      bytes_usage(bytes_usage),
      last_used(last_used) {}
EvictionCandidateAggregator::EvictionCandidate::~EvictionCandidate() = default;
EvictionCandidateAggregator::EvictionCandidate::EvictionCandidate(
    EvictionCandidate&&) = default;
EvictionCandidateAggregator::EvictionCandidate&
EvictionCandidateAggregator::EvictionCandidate::operator=(EvictionCandidate&&) =
    default;

EvictionCandidateAggregator::EvictionCandidateAggregator(
    int64_t size_to_be_removed,
    std::vector<scoped_refptr<base::SequencedTaskRunner>> task_runners)
    : size_to_be_removed_(size_to_be_removed),
      task_runners_(std::move(task_runners)),
      selected_callbacks_(task_runners_.size()) {
  candidates_per_shard_.reserve(task_runners_.size());
}

void EvictionCandidateAggregator::OnCandidate(
    SqlPersistentStore::ShardId shard_id,
    EvictionCandidateList candidate,
    EvictionCandidateSelectedCallback selected_callback) {
  CHECK(task_runners_[shard_id.value()]->RunsTasksInCurrentSequence());
  std::vector<EvictionCandidateList> candidates_per_shard;
  std::vector<EvictionCandidateSelectedCallback> selected_callbacks;
  if (!AddCandidates(shard_id, std::move(candidate),
                     std::move(selected_callback), candidates_per_shard,
                     selected_callbacks)) {
    return;
  }
  AggregateCandidatesAndRunCallbacks(std::move(candidates_per_shard),
                                     std::move(selected_callbacks), shard_id);
}

EvictionCandidateAggregator::~EvictionCandidateAggregator() = default;

bool EvictionCandidateAggregator::AddCandidates(
    SqlPersistentStore::ShardId shard_id,
    EvictionCandidateList new_candidates,
    EvictionCandidateSelectedCallback new_selected_callback,
    std::vector<EvictionCandidateList>& candidates_per_shard_out,
    std::vector<EvictionCandidateSelectedCallback>& selected_callbacks_out) {
  base::AutoLock auto_lock(lock_);
  candidates_per_shard_.emplace_back(std::move(new_candidates));
  selected_callbacks_[shard_id.value()] = std::move(new_selected_callback);
  if (candidates_per_shard_.size() != GetSizeOfShards()) {
    return false;
  }
  candidates_per_shard_out = std::move(candidates_per_shard_);
  selected_callbacks_out = std::move(selected_callbacks_);
  return true;
}

void EvictionCandidateAggregator::AggregateCandidatesAndRunCallbacks(
    std::vector<EvictionCandidateList> candidates_per_shard,
    std::vector<EvictionCandidateSelectedCallback> selected_callbacks,
    SqlPersistentStore::ShardId last_shard_id) {
  size_t all_candidate_count = 0;
  for (const auto& candidates : candidates_per_shard) {
    all_candidate_count += candidates.size();
  }
  EvictionCandidateList all_candidates;
  all_candidates.reserve(all_candidate_count);
  for (auto& candidates : candidates_per_shard) {
    all_candidates.insert(all_candidates.end(),
                          std::make_move_iterator(candidates.begin()),
                          std::make_move_iterator(candidates.end()));
  }
  // Sort candidates by last_used time, oldest first.
  std::sort(
      all_candidates.begin(), all_candidates.end(),
      [](const auto& a, const auto& b) { return a.last_used < b.last_used; });

  std::vector<std::vector<SqlPersistentStore::ResId>>
      selected_res_ids_per_shard(GetSizeOfShards());
  std::vector<int64_t> bytes_usage_per_shard(GetSizeOfShards());
  int64_t removed_total_size = 0;
  for (const EvictionCandidate& candidate : all_candidates) {
    removed_total_size += candidate.bytes_usage;
    bytes_usage_per_shard[candidate.shard_id.value()] += candidate.bytes_usage;
    selected_res_ids_per_shard[candidate.shard_id.value()].emplace_back(
        candidate.res_id);
    if (removed_total_size > size_to_be_removed_) {
      break;
    }
  }

  // Post the eviction tasks back to each shard.
  for (size_t i = 0; i < GetSizeOfShards(); ++i) {
    if (i == last_shard_id.value()) {
      // The last shard's callback is run on the current thread, so we skip
      // it here.
      continue;
    }
    task_runners_[i]->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(selected_callbacks[i]),
                       std::move(selected_res_ids_per_shard[i]),
                       bytes_usage_per_shard[i], base::TimeTicks::Now()));
  }

  // Run the last shard's callback on the current thread to avoid an
  // unnecessary thread hop.
  DCHECK(task_runners_[last_shard_id.value()]->RunsTasksInCurrentSequence());
  std::move(selected_callbacks[last_shard_id.value()])
      .Run(std::move(selected_res_ids_per_shard[last_shard_id.value()]),
           bytes_usage_per_shard[last_shard_id.value()],
           base::TimeTicks::Now());
}

size_t EvictionCandidateAggregator::GetSizeOfShards() const {
  return task_runners_.size();
}

}  // namespace disk_cache
