// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/eviction_candidate_aggregator.h"

#include <vector>

#include "base/barrier_closure.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace disk_cache {

namespace {

using ResId = SqlPersistentStore::ResId;
using ShardId = SqlPersistentStore::ShardId;
using EvictionCandidateList =
    EvictionCandidateAggregator::EvictionCandidateList;
using EvictionTarget = SqlPersistentStore::EvictionTarget;

std::vector<EvictionTarget> QueueToVector(
    SqlPersistentStore::EvictionTargetQueue queue) {
  std::vector<EvictionTarget> vec;
  vec.reserve(queue.size());
  while (!queue.empty()) {
    vec.push_back(queue.front());
    queue.pop();
  }
  return vec;
}

class EvictionCandidateAggregatorTest : public testing::Test {
 public:
  EvictionCandidateAggregatorTest() = default;
  ~EvictionCandidateAggregatorTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;
};

// Tests that candidates are sorted by sort_value, highest first.
TEST_F(EvictionCandidateAggregatorTest, SortsBySortValue) {
  const int kNumShards = 2;
  std::vector<scoped_refptr<base::SequencedTaskRunner>> task_runners;
  for (int i = 0; i < kNumShards; ++i) {
    task_runners.push_back(base::ThreadPool::CreateSequencedTaskRunner({}));
  }

  // We need to remove 100 bytes, so the two entries with highest sort_values
  // should be selected.
  const int64_t kSizeToBeRemoved = 100;
  auto aggregator = base::MakeRefCounted<EvictionCandidateAggregator>(
      kSizeToBeRemoved, task_runners);

  EvictionCandidateList candidates0;
  // Highest sort_value candidate.
  candidates0.emplace_back(ResId(1), ShardId(0), 50, 100);
  // 4th highest sort_value candidate.
  candidates0.emplace_back(ResId(2), ShardId(0), 50, 70);

  EvictionCandidateList candidates1;
  // 2nd highest sort_value candidate.
  candidates1.emplace_back(ResId(3), ShardId(1), 60, 90);
  // 3rd highest sort_value candidate.
  candidates1.emplace_back(ResId(4), ShardId(1), 60, 80);

  base::RunLoop run_loop;
  auto on_done = base::BarrierClosure(kNumShards, run_loop.QuitClosure());

  auto cb0 = base::BindOnce(
      [](base::OnceClosure on_done,
         SqlPersistentStore::EvictionTargetQueue eviction_targets,
         base::TimeTicks post_task_time) {
        // This shard had the highest sort_value candidate (ResId(1)).
        EXPECT_THAT(QueueToVector(std::move(eviction_targets)),
                    testing::ElementsAre(EvictionTarget(ResId(1), 50)));
        std::move(on_done).Run();
      },
      on_done);
  auto cb1 = base::BindOnce(
      [](base::OnceClosure on_done,
         SqlPersistentStore::EvictionTargetQueue eviction_targets,
         base::TimeTicks post_task_time) {
        // This shard had the second oldest candidate (ResId(3)).
        EXPECT_THAT(QueueToVector(std::move(eviction_targets)),
                    testing::ElementsAre(EvictionTarget(ResId(3), 60)));
        std::move(on_done).Run();
      },
      on_done);

  task_runners[0]->PostTask(
      FROM_HERE,
      base::BindOnce(&EvictionCandidateAggregator::OnCandidate, aggregator,
                     ShardId(0), std::move(candidates0), std::move(cb0)));
  task_runners[1]->PostTask(
      FROM_HERE,
      base::BindOnce(&EvictionCandidateAggregator::OnCandidate, aggregator,
                     ShardId(1), std::move(candidates1), std::move(cb1)));
  run_loop.Run();
}

// Tests that the aggregator selects just enough candidates to meet the
// size_to_be_removed requirement.
TEST_F(EvictionCandidateAggregatorTest, SelectsEnoughToRemove) {
  std::vector<scoped_refptr<base::SequencedTaskRunner>> task_runners;
  task_runners.push_back(base::ThreadPool::CreateSequencedTaskRunner({}));

  // We need to remove 100 bytes. The two entries with highest sort_values sum
  // to 90 (40+50), so the third one (50 bytes) must also be selected, bringing
  // the total to 140.
  const int64_t kSizeToBeRemoved = 100;
  auto aggregator = base::MakeRefCounted<EvictionCandidateAggregator>(
      kSizeToBeRemoved, task_runners);

  EvictionCandidateList candidates;
  candidates.emplace_back(ResId(1), ShardId(0), 40, 100);
  candidates.emplace_back(ResId(2), ShardId(0), 50, 90);
  candidates.emplace_back(ResId(3), ShardId(0), 50, 80);
  candidates.emplace_back(ResId(4), ShardId(0), 80, 70);

  base::RunLoop run_loop;
  auto cb = base::BindOnce(
      [](base::OnceClosure on_done,
         SqlPersistentStore::EvictionTargetQueue eviction_targets,
         base::TimeTicks post_task_time) {
        EXPECT_THAT(QueueToVector(std::move(eviction_targets)),
                    testing::ElementsAre(EvictionTarget(ResId(1), 40),
                                         EvictionTarget(ResId(2), 50),
                                         EvictionTarget(ResId(3), 50)));
        std::move(on_done).Run();
      },
      run_loop.QuitClosure());

  task_runners[0]->PostTask(
      FROM_HERE,
      base::BindOnce(&EvictionCandidateAggregator::OnCandidate, aggregator,
                     ShardId(0), std::move(candidates), std::move(cb)));

  run_loop.Run();
}

// Tests that the aggregator works correctly with multiple task runners.
TEST_F(EvictionCandidateAggregatorTest, HandlesMultipleSequences) {
  const int kNumShards = 3;

  std::vector<scoped_refptr<base::SequencedTaskRunner>> task_runners;
  for (int i = 0; i < kNumShards; ++i) {
    task_runners.push_back(base::ThreadPool::CreateSequencedTaskRunner({}));
  }

  const int64_t kSizeToBeRemoved = 150;
  auto aggregator = base::MakeRefCounted<EvictionCandidateAggregator>(
      kSizeToBeRemoved, task_runners);

  EvictionCandidateList candidates0;
  candidates0.emplace_back(ResId(1), ShardId(0), 50, 100);
  candidates0.emplace_back(ResId(2), ShardId(0), 50, 50);

  EvictionCandidateList candidates1;
  candidates1.emplace_back(ResId(3), ShardId(1), 60, 90);

  EvictionCandidateList candidates2;
  candidates2.emplace_back(ResId(4), ShardId(2), 70, 80);
  candidates2.emplace_back(ResId(5), ShardId(2), 10, 70);

  base::RunLoop run_loop;
  auto on_done = base::BarrierClosure(kNumShards, run_loop.QuitClosure());

  auto cb0 = base::BindOnce(
      [](scoped_refptr<base::SequencedTaskRunner> runner,
         base::OnceClosure on_done,
         SqlPersistentStore::EvictionTargetQueue eviction_targets,
         base::TimeTicks post_task_time) {
        EXPECT_TRUE(runner->RunsTasksInCurrentSequence());
        EXPECT_THAT(QueueToVector(std::move(eviction_targets)),
                    testing::ElementsAre(EvictionTarget(ResId(1), 50)));
        std::move(on_done).Run();
      },
      task_runners[0], on_done);

  auto cb1 = base::BindOnce(
      [](scoped_refptr<base::SequencedTaskRunner> runner,
         base::OnceClosure on_done,
         SqlPersistentStore::EvictionTargetQueue eviction_targets,
         base::TimeTicks post_task_time) {
        EXPECT_TRUE(runner->RunsTasksInCurrentSequence());
        EXPECT_THAT(QueueToVector(std::move(eviction_targets)),
                    testing::ElementsAre(EvictionTarget(ResId(3), 60)));
        std::move(on_done).Run();
      },
      task_runners[1], on_done);

  auto cb2 = base::BindOnce(
      [](scoped_refptr<base::SequencedTaskRunner> runner,
         base::OnceClosure on_done,
         SqlPersistentStore::EvictionTargetQueue eviction_targets,
         base::TimeTicks post_task_time) {
        EXPECT_TRUE(runner->RunsTasksInCurrentSequence());
        EXPECT_THAT(QueueToVector(std::move(eviction_targets)),
                    testing::ElementsAre(EvictionTarget(ResId(4), 70)));
        std::move(on_done).Run();
      },
      task_runners[2], on_done);

  task_runners[0]->PostTask(
      FROM_HERE,
      base::BindOnce(&EvictionCandidateAggregator::OnCandidate, aggregator,
                     ShardId(0), std::move(candidates0), std::move(cb0)));
  task_runners[1]->PostTask(
      FROM_HERE,
      base::BindOnce(&EvictionCandidateAggregator::OnCandidate, aggregator,
                     ShardId(1), std::move(candidates1), std::move(cb1)));
  task_runners[2]->PostTask(
      FROM_HERE,
      base::BindOnce(&EvictionCandidateAggregator::OnCandidate, aggregator,
                     ShardId(2), std::move(candidates2), std::move(cb2)));

  run_loop.Run();
}

}  // namespace

}  // namespace disk_cache
