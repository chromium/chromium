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

BASE_FEATURE(kIdbInSessionDbCleanup, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kIdbVerifyInSessionDbCleanup, base::FEATURE_DISABLED_BY_DEFAULT);

namespace {
constexpr base::TimeDelta kTimeBetweenRuns = base::Minutes(30);
}  // namespace

LevelDBCleanupScheduler::LevelDBCleanupScheduler(leveldb::DB* database,
                                                 Delegate* delegate)
    : database_(database),
      delegate_(delegate),
      // The last_run_ is initialized to a past value so short lived apps
      // which accrue a lot of tombstones will get a chance to have their
      // tombstones cleaned.
      last_run_(base::TimeTicks::Min()) {}

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
  if (running_state_ &&
      running_state_->clean_up_scheduling_timer_.IsRunning()) {
    ++running_state_->postpone_count;
    running_state_->clean_up_scheduling_timer_.Stop();
  }
}

void LevelDBCleanupScheduler::OnTransactionComplete() {
  --active_transactions_count_;
  if (active_transactions_count_ == 0 && running_state_ &&
      !running_state_->clean_up_scheduling_timer_.IsRunning()) {
    // Schedule a run if there are no active transactions and
    // `kTimeBetweenRuns` has been exceeded. The check for `time_since_last_run`
    // is only required when it's the first time the run is being scheduled
    // and has not been paused by `OnTransactionStart`. However, it will
    // always be true if the run was paused, as the condition was met when
    // it was scheduled the first time.
    base::TimeDelta time_since_last_run = base::TimeTicks::Now() - last_run_;
    if (time_since_last_run > kTimeBetweenRuns) {
      ScheduleNextCleanupTask();
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

  running_state_.emplace();
}

void LevelDBCleanupScheduler::ScheduleNextCleanupTask() {
  CHECK(running_state_);
  running_state_->clean_up_scheduling_timer_.Start(
      FROM_HERE, kDeferTime,
      base::BindRepeating(&LevelDBCleanupScheduler::RunCleanupTask,
                          base::Unretained(this)));
}

void LevelDBCleanupScheduler::RunCleanupTask() {
  CHECK(running_state_);

  bool tombstone_sweeper_run_complete = false;
  base::TimeTicks time_before_round = base::TimeTicks::Now();
  switch (running_state_->cleanup_phase) {
    case Phase::kRunScheduled:
      delegate_->OnCleanupStarted();
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
      ScheduleNextCleanupTask();
      break;
    case Phase::kDatabaseCompaction:
      IndexedDBCompactionTask(database_).RunRound();
      running_state_->db_compaction_duration =
          base::TimeTicks::Now() - time_before_round;
      base::UmaHistogramTimes(
          "IndexedDB.LevelDBCleanupScheduler.DBCompactionDuration",
          running_state_->db_compaction_duration);
      running_state_->cleanup_phase = Phase::kLoggingAndCleanup;
      ScheduleNextCleanupTask();
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

  delegate_->OnCleanupDone();
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
