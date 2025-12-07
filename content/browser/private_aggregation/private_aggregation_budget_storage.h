// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_BUDGET_STORAGE_H_
#define CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_BUDGET_STORAGE_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/sqlite_proto/key_value_data.h"
#include "components/sqlite_proto/key_value_table.h"
#include "content/common/content_export.h"

namespace base {
class ElapsedTimer;
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace sql {
class Database;
}  // namespace sql

namespace sqlite_proto {
class ProtoTableManager;
}  // namespace sqlite_proto

namespace content {

namespace proto {
class PrivateAggregationBudgets;
}  // namespace proto

// UI thread class that provides an interface for storing the budget used by
// each key, i.e. the sum of contributions, and persisting that to an on-disk
// database (unless run exclusively in memory). This class is responsible for
// owning and initializing the database and sqlite_proto classes. Note that
// callbacks are used for storage return values to allow switching the
// underlying storage in the future in case the cache uses too much memory.
// Writes are buffered for `kFlushDelay` and then persisted to disk. The
// `Create()` factory method ensures that this class stays alive through
// initialization; after that point, it has no specific lifetime requirements.
class CONTENT_EXPORT PrivateAggregationBudgetStorage {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class InitStatus {
    kSuccess = 0,
    kFailedToOpenDbInMemory = 1,
    kFailedToOpenDbFile = 2,
    kFailedToCreateDir = 3,
    kMaxValue = kFailedToCreateDir,
  };

  // Constructs and asynchronously initializes a new
  // `PrivateAggregationBudgetStorage`, including posting a task to
  // `db_task_runner` to initialize the underlying database on its sequence.
  // Calls `on_done_initializing` once initialization is complete, passing an
  // owning pointer if initialization was successful and nullptr otherwise. If
  // `exclusively_run_in_memory` is `true`, the database will not be persisted
  // to disk. Returns a closure that shuts down the storage before it is
  // finished initializing. This is necessary to avoid posting tasks after
  // shutdown if initialization finishes too late.
  static base::OnceClosure CreateAsync(
      scoped_refptr<base::SequencedTaskRunner> db_task_runner,
      bool exclusively_run_in_memory,
      base::FilePath path_to_db_dir,
      base::OnceCallback<void(std::unique_ptr<PrivateAggregationBudgetStorage>)>
          on_done_initializing);

  PrivateAggregationBudgetStorage(
      const PrivateAggregationBudgetStorage& other) = delete;
  PrivateAggregationBudgetStorage& operator=(
      const PrivateAggregationBudgetStorage& other) = delete;
  ~PrivateAggregationBudgetStorage();

  // The maximum time writes will be buffered before being committed to disk.
  // Note that `Shutdown()` will flush pending writes to disk without delay.
  // This value was chosen to match TrustTokenDatabaseOwner.
  static constexpr base::TimeDelta kFlushDelay = base::Seconds(2);

  // TODO(crbug.com/40226450): Support data deletion.

  sqlite_proto::KeyValueData<proto::PrivateAggregationBudgets>* budgets_data() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return &budgets_data_;
  }

  // Asynchronously tears down the budget storage database. Can be called before
  // initialization is complete. This is necessary to avoid posting tasks after
  // shutdown if initialization finishes too late.
  void Shutdown();

 private:
  explicit PrivateAggregationBudgetStorage(
      scoped_refptr<base::SequencedTaskRunner> db_task_runner);

  // Creates and opens the database and then calls into ProtoTableManager's and
  // KeyValueData's initialization methods. If `exclusively_run_in_memory` is
  // true, the underlying database will be in-memory and not be persisted to
  // disk. Otherwise, a copy of the data is kept in-memory, but regularly
  // flushed to disk. Takes a raw pointer to the database in case `db_` is
  // released before or while this is running.
  [[nodiscard]] bool InitializeOnDbSequence(sql::Database* db,
                                            bool exclusively_run_in_memory,
                                            base::FilePath path_to_db_dir);

  void FinishInitializationOnMainSequence(
      std::unique_ptr<PrivateAggregationBudgetStorage> owned_this,
      base::OnceCallback<void(std::unique_ptr<PrivateAggregationBudgetStorage>)>
          on_done_initializing,
      base::ElapsedTimer elapsed_timer,
      bool was_successful);

  scoped_refptr<sqlite_proto::ProtoTableManager> table_manager_;

  std::unique_ptr<sqlite_proto::KeyValueTable<proto::PrivateAggregationBudgets>>
      budgets_table_;
  sqlite_proto::KeyValueData<proto::PrivateAggregationBudgets> budgets_data_;

  // Keep a handle on the DB task runner so that the destructor can use the DB
  // sequence to clean up the DB.
  scoped_refptr<base::SequencedTaskRunner> db_task_runner_;

  // Created on the main sequence, but otherwise should only be accessed on the
  // `db_task_runner_` sequence. It is also released (but not deleted) on the
  // main sequence.
  std::unique_ptr<sql::Database> db_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<PrivateAggregationBudgetStorage> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_BUDGET_STORAGE_H_
