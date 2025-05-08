// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/leveldb/cleanup_scheduler.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "content/browser/indexed_db/instance/leveldb/compaction_task.h"
#include "content/browser/indexed_db/instance/leveldb/tombstone_sweeper.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_metadata.h"

namespace content::indexed_db::level_db {

BASE_FEATURE(kIdbInSessionDbCleanup,
             "IdbInSessionDbCleanup",
             base::FEATURE_DISABLED_BY_DEFAULT);

namespace {
constexpr base::TimeDelta kMaximumTimeBetweenRuns = base::Minutes(50);
constexpr base::TimeDelta kMinimumTimeBetweenRuns = base::Minutes(30);

// To ensure the sweeper runs for database connections with a shorter
// timespan, run the first sweep after `kMinimumTimeBeforeInitialForcedRun`
// minutes.
// The last_run_ is initialized to 40 minutes in the past so short lived apps
// which accrue a lot of tombstones will get a chance to have their tombstones
// cleaned. The first sweep can be happen if there are no active transactions in
// last 4 seconds and can get postponed for another 10 minutues due to database
// activity. But after the 10 minute mark, the cleanup will be forced once the
// current transaction count reaches zero.
constexpr base::TimeDelta kMinimumTimeBeforeInitialForcedRun =
    base::Minutes(10);
}  // namespace

LevelDBCleanupScheduler::LevelDBCleanupScheduler(leveldb::DB* database,
                                                 Delegate* delegate)
    : database_(database),
      delegate_(delegate),
      last_run_(base::TimeTicks::Now() - kMaximumTimeBetweenRuns +
                kMinimumTimeBeforeInitialForcedRun) {}

LevelDBCleanupScheduler::~LevelDBCleanupScheduler() {
  if (running_state_) {
    base::UmaHistogramEnumeration(
        "IndexedDB.LevelDBCleanupScheduler.PrematureTerminationPhase",
        running_state_->cleanup_phase);
    base::UmaHistogramCounts10000(
        "IndexedDB.LevelDBCleanupScheduler.CleanerPostponedCount",
        running_state_->postpone_count);
  }
}

void LevelDBCleanupScheduler::OnTransactionStart() {
  ++active_transactions_count_;
  // Do not stop the cleanup if it's past `kMaximumTimeBetweenRuns`
  if (running_state_ &&
      running_state_->clean_up_scheduling_timer_.IsRunning() &&
      (base::TimeTicks::Now() - last_run_) < kMaximumTimeBetweenRuns) {
    ++running_state_->postpone_count;
    running_state_->clean_up_scheduling_timer_.Stop();
  }
}

void LevelDBCleanupScheduler::OnTransactionComplete() {
  --active_transactions_count_;
  bool scheduler_waiting_to_schedule =
      running_state_ && !running_state_->clean_up_scheduling_timer_.IsRunning();
  if (scheduler_waiting_to_schedule) {
    // Schedule a run if there are no active transactions or the upper bound has
    // been exceeded.
    if (active_transactions_count_ == 0 ||
        (base::TimeTicks::Now() - last_run_) > kMaximumTimeBetweenRuns) {
      ScheduleNextCleanupTask(kDeferTimeAfterLastTransaction);
    }
  }
}

void LevelDBCleanupScheduler::Initialize() {
  if (!base::FeatureList::IsEnabled(kIdbInSessionDbCleanup)) {
    return;
  }

  if (running_state_) {
    return;
  }

  // Initialize the `running_state` if it has been more than
  // `kMinimumTimeBetweenRuns` since the last run.
  if ((base::TimeTicks::Now() - last_run_) > kMinimumTimeBetweenRuns) {
    running_state_.emplace();
  }
}

void LevelDBCleanupScheduler::ScheduleNextCleanupTask(
    const base::TimeDelta& defer_time) {
  CHECK(running_state_);
  running_state_->clean_up_scheduling_timer_.Start(
      FROM_HERE, defer_time,
      base::BindRepeating(&LevelDBCleanupScheduler::RunCleanupTask,
                          base::Unretained(this)));
}

void LevelDBCleanupScheduler::RunCleanupTask() {
  CHECK(running_state_);

  bool tombstone_sweeper_run_complete = false;
  base::TimeTicks time_before_round = base::TimeTicks::Now();
  switch (running_state_->cleanup_phase) {
    case Phase::kRunScheduled:
      running_state_->cleanup_phase = Phase::kTombstoneSweeper;
      ABSL_FALLTHROUGH_INTENDED;
    case Phase::kTombstoneSweeper:
      tombstone_sweeper_run_complete = RunTombstoneSweeper();
      running_state_->tombstone_sweeper_duration +=
          base::TimeTicks::Now() - time_before_round;

      // If not `tombstone_sweeper_run_complete`, we stay in this
      // `kTombstoneSweeper` phase and schedule another round of tombstone
      // sweeper. If done, we reset the timer and move to db compaction.
      if (tombstone_sweeper_run_complete) {
        base::UmaHistogramTimes(
            "IndexedDB.LevelDBCleanupScheduler.TombstoneSweeperDuration",
            running_state_->tombstone_sweeper_duration);
        running_state_->cleanup_phase = Phase::kDatabaseCompaction;
      }
      ScheduleNextCleanupTask(kDeferTimeOnNoTransactions);
      break;
    case Phase::kDatabaseCompaction:
      IndexedDBCompactionTask(database_).RunRound();
      running_state_->db_compaction_duration =
          base::TimeTicks::Now() - time_before_round;
      base::UmaHistogramTimes(
          "IndexedDB.LevelDBCleanupScheduler.DBCompactionDuration",
          running_state_->db_compaction_duration);
      running_state_->cleanup_phase = Phase::kLoggingAndCleanup;
      ScheduleNextCleanupTask(kDeferTimeOnNoTransactions);
      break;
    case Phase::kLoggingAndCleanup:
      LogAndResetState();
      break;
  }
}

void LevelDBCleanupScheduler::LogAndResetState() {
  CHECK(running_state_);

  base::UmaHistogramCounts10000(
      "IndexedDB.LevelDBCleanupScheduler.CleanerPostponedCount",
      running_state_->postpone_count);

  // Reset the cleanup state
  last_run_ = base::TimeTicks::Now();
  running_state_.reset();

  // Update the timers for traditional sweeper.
  delegate_->UpdateEarliestSweepTime();
  delegate_->UpdateEarliestCompactionTime();
}

bool LevelDBCleanupScheduler::RunTombstoneSweeper() {
  CHECK(running_state_);
  if (!running_state_->tombstone_sweeper) {
    delegate_->GetCompleteMetadata(&running_state_->metadata_vector);
    running_state_->tombstone_sweeper =
        std::make_unique<LevelDbTombstoneSweeper>(database_);
    running_state_->tombstone_sweeper->SetMetadata(
        &running_state_->metadata_vector);
  }

  return running_state_->tombstone_sweeper->RunRound();
}

LevelDBCleanupScheduler::RunningState::RunningState() = default;
LevelDBCleanupScheduler::RunningState::~RunningState() = default;

}  // namespace content::indexed_db::level_db
