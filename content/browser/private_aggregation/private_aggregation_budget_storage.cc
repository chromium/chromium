// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/private_aggregation/private_aggregation_budget_storage.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/clamped_math.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/elapsed_timer.h"
#include "components/sqlite_proto/key_value_data.h"
#include "components/sqlite_proto/key_value_table.h"
#include "components/sqlite_proto/proto_table_manager.h"
#include "content/browser/private_aggregation/proto/private_aggregation_budgets.pb.h"
#include "sql/database.h"

namespace content {

namespace {

constexpr base::FilePath::CharType kDatabaseFilename[] =
    FILE_PATH_LITERAL("PrivateAggregation");

constexpr char kBudgetsTableName[] = "private_aggregation_api_budgets";

// When updating the database's schema, please increment the schema version.
// This will raze the database. This is not necessary for backwards-compatible
// updates to the proto format.
constexpr int kCurrentSchemaVersion = 2;

void RecordInitializationStatus(
    PrivateAggregationBudgetStorage::InitStatus status) {
  base::UmaHistogramEnumeration(
      "PrivacySandbox.PrivateAggregation.BudgetStorage.InitStatus", status);
}

void RecordFileSizeHistogram(const base::FilePath& path_to_database) {
  if (int64_t size_bytes; base::GetFileSize(path_to_database, &size_bytes)) {
    base::UmaHistogramCounts1M(
        "PrivacySandbox.PrivateAggregation.BudgetStorage.DbSize",
        base::MakeClampedNum(size_bytes / 1024));
  }
}

}  // namespace

// static
base::OnceClosure PrivateAggregationBudgetStorage::CreateAsync(
    scoped_refptr<base::SequencedTaskRunner> db_task_runner,
    bool exclusively_run_in_memory,
    base::FilePath path_to_db_dir,
    base::OnceCallback<void(std::unique_ptr<PrivateAggregationBudgetStorage>)>
        on_done_initializing) {
  CHECK(on_done_initializing);
  base::UmaHistogramBoolean(
      "PrivacySandbox.PrivateAggregation.BudgetStorage."
      "BeginInitializationCount",
      /*sample=*/true);
  auto storage =
      base::WrapUnique(new PrivateAggregationBudgetStorage(db_task_runner));
  auto* raw_storage = storage.get();

  // `base::Unretained` is safe here as it is impossible for `storage` to be
  // deleted before initialization finishes as it is now owned by the reply
  // callback below, i.e. passed to `FinishInitializationOnMainSequence()`.
  // Similarly, passing the database raw pointer is safe as it can only be
  // destroyed on the database sequence after `InitializeOnDbSequence()`.
  db_task_runner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&PrivateAggregationBudgetStorage::InitializeOnDbSequence,
                     base::Unretained(raw_storage),
                     /*db=*/raw_storage->db_.get(), exclusively_run_in_memory,
                     std::move(path_to_db_dir)),
      base::BindOnce(
          &PrivateAggregationBudgetStorage::FinishInitializationOnMainSequence,
          base::Unretained(raw_storage), std::move(storage),
          std::move(on_done_initializing), base::ElapsedTimer()));

  return base::BindOnce(&PrivateAggregationBudgetStorage::Shutdown,
                        raw_storage->weak_factory_.GetWeakPtr());
}

PrivateAggregationBudgetStorage::PrivateAggregationBudgetStorage(
    scoped_refptr<base::SequencedTaskRunner> db_task_runner)
    : table_manager_(base::MakeRefCounted<sqlite_proto::ProtoTableManager>(
          db_task_runner)),
      budgets_table_(
          std::make_unique<
              sqlite_proto::KeyValueTable<proto::PrivateAggregationBudgets>>(
              kBudgetsTableName)),
      budgets_data_(table_manager_,
                    budgets_table_.get(),
                    /*max_num_entries=*/std::nullopt,
                    kFlushDelay),
      db_task_runner_(std::move(db_task_runner)),
      db_(std::make_unique<sql::Database>(
          sql::DatabaseOptions{.page_size = 4096, .cache_size = 32})) {}

PrivateAggregationBudgetStorage::~PrivateAggregationBudgetStorage() {
  Shutdown();
}

bool PrivateAggregationBudgetStorage::InitializeOnDbSequence(
    sql::Database* db,
    bool exclusively_run_in_memory,
    base::FilePath path_to_db_dir) {
  CHECK(db_task_runner_->RunsTasksInCurrentSequence());
  CHECK(db);

  db->set_histogram_tag("PrivateAggregation");

  // TODO(crbug.com/40224647): Record histograms for the different
  // outcomes/errors.
  if (exclusively_run_in_memory) {
    if (!db->OpenInMemory()) {
      RecordInitializationStatus(InitStatus::kFailedToOpenDbInMemory);
      return false;
    }
  } else {
    const bool dir_exists_or_was_created =
        base::DirectoryExists(path_to_db_dir) ||
        base::CreateDirectory(path_to_db_dir);
    if (!dir_exists_or_was_created) {
      RecordInitializationStatus(InitStatus::kFailedToCreateDir);
      return false;
    }
    base::FilePath path_to_database = path_to_db_dir.Append(kDatabaseFilename);
    if (!db->Open(path_to_database)) {
      RecordInitializationStatus(InitStatus::kFailedToOpenDbFile);
      return false;
    }
    RecordFileSizeHistogram(path_to_database);
  }

  table_manager_->InitializeOnDbSequence(
      db, std::vector<std::string>{kBudgetsTableName}, kCurrentSchemaVersion);

  budgets_data_.InitializeOnDBSequence();

  RecordInitializationStatus(InitStatus::kSuccess);
  return true;
}

void PrivateAggregationBudgetStorage::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(!!db_, !!budgets_table_);

  // Guard against `Shutdown()` being called multiple times.
  if (db_) {
    // Schedules `budgets_table_` tasks on the DB sequence for all pending
    // updates. Since they are scheduled before the `DeleteSoon()` commands, the
    // tasks will be able to complete before `budgets_table_` gets destroyed.
    budgets_data_.FlushDataToDisk();

    // The following protects `table_manager_` from holding a dangling pointer
    // to `db_`. This is possible in the case that the
    // PrivateAggregationBudgeter is deleted before `this` is finished
    // initializing. In that case, we can delete the `db_` before the
    // `table_manager_` is deleted.
    // TODO(alexmt,csharrison): Consider refactoring the ownership here.
    db_task_runner_->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&sqlite_proto::ProtoTableManager::WillShutdown,
                       base::Unretained(table_manager_.get())),
        base::BindOnce([](sqlite_proto::ProtoTableManager*) {},
                       base::RetainedRef(table_manager_)));

    // `budgets_table_` must be deleted on the database sequence.
    db_task_runner_->DeleteSoon(FROM_HERE, budgets_table_.release());

    // The sequenced task runner will ensure that this `db_` destruction task
    // doesn't run until after `InitializeOnDbSequence()` runs.
    db_task_runner_->DeleteSoon(FROM_HERE, db_.release());
  }
}

void PrivateAggregationBudgetStorage::FinishInitializationOnMainSequence(
    std::unique_ptr<PrivateAggregationBudgetStorage> owned_this,
    base::OnceCallback<void(std::unique_ptr<PrivateAggregationBudgetStorage>)>
        on_done_initializing,
    base::ElapsedTimer elapsed_timer,
    bool was_successful) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(owned_this);

  base::UmaHistogramBoolean(
      "PrivacySandbox.PrivateAggregation.BudgetStorage."
      "ShutdownBeforeFinishingInitialization",
      !db_);
  base::UmaHistogramTimes(
      "PrivacySandbox.PrivateAggregation.BudgetStorage.InitTime",
      elapsed_timer.Elapsed());

  // If the initialization failed, `this` will be destroyed after its unique_ptr
  // passes out of scope here.
  std::move(on_done_initializing)
      .Run(was_successful ? std::move(owned_this) : nullptr);
}

}  // namespace content
