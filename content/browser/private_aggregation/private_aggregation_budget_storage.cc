// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/private_aggregation/private_aggregation_budget_storage.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/sqlite_proto/key_value_data.h"
#include "components/sqlite_proto/key_value_table.h"
#include "components/sqlite_proto/proto_table_manager.h"
#include "content/browser/private_aggregation/proto/private_aggregation_budgets.pb.h"
#include "sql/database.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

namespace {

constexpr base::FilePath::CharType kDatabaseFilename[] =
    FILE_PATH_LITERAL("PrivateAggregation");

constexpr char kBudgetsTableName[] = "private_aggregation_api_budgets";

// When updating the database's schema, please increment the schema version.
// This will raze the database. This is not necessary for backwards-compatible
// updates to the proto format.
// TODO(crbug.com/1335490): Add presubmit to enforce updating.
constexpr int kCurrentSchemaVersion = 1;

}  // namespace

// static
void PrivateAggregationBudgetStorage::CreateAsync(
    scoped_refptr<base::SequencedTaskRunner> db_task_runner,
    bool exclusively_run_in_memory,
    base::FilePath path_to_db_dir,
    base::OnceCallback<void(std::unique_ptr<PrivateAggregationBudgetStorage>)>
        on_done_initializing) {
  DCHECK(on_done_initializing);
  auto storage =
      base::WrapUnique(new PrivateAggregationBudgetStorage(db_task_runner));
  auto* raw_storage = storage.get();

  // `base::Unretained` is safe here as it is impossible for `storage` to be
  // deleted before initialization finishes as it is now owned by the reply
  // callback below, i.e. passed to `FinishInitializationOnMainSequence()`.
  db_task_runner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&PrivateAggregationBudgetStorage::InitializeOnDbSequence,
                     base::Unretained(raw_storage), exclusively_run_in_memory,
                     std::move(path_to_db_dir)),
      base::BindOnce(
          &PrivateAggregationBudgetStorage::FinishInitializationOnMainSequence,
          base::Unretained(raw_storage), /*owned_this=*/std::move(storage),
          std::move(on_done_initializing)));
}

PrivateAggregationBudgetStorage::PrivateAggregationBudgetStorage(
    scoped_refptr<base::SequencedTaskRunner> db_task_runner)
    : table_manager_(base::MakeRefCounted<sqlite_proto::ProtoTableManager>(
          db_task_runner)),
      budgets_table_(kBudgetsTableName),
      budgets_data_(table_manager_,
                    &budgets_table_,
                    /*max_num_entries=*/absl::nullopt,
                    kFlushDelay),
      db_task_runner_(std::move(db_task_runner)) {}

PrivateAggregationBudgetStorage::~PrivateAggregationBudgetStorage() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  db_task_runner_->DeleteSoon(FROM_HERE, db_.release());
}

bool PrivateAggregationBudgetStorage::InitializeOnDbSequence(
    bool exclusively_run_in_memory,
    base::FilePath path_to_db_dir) {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());

  db_ = std::make_unique<sql::Database>(sql::DatabaseOptions{
      .exclusive_locking = true, .page_size = 4096, .cache_size = 32});

  db_->set_histogram_tag("PrivateAggregation");

  if (exclusively_run_in_memory) {
    if (!db_->OpenInMemory()) {
      HandleInitializationFailure();
      return false;
    }
  } else {
    const bool dir_exists_or_was_created =
        base::DirectoryExists(path_to_db_dir) ||
        base::CreateDirectory(path_to_db_dir);
    if (!dir_exists_or_was_created) {
      HandleInitializationFailure();
      return false;
    }
    base::FilePath path_to_database = path_to_db_dir.Append(kDatabaseFilename);
    if (!db_->Open(path_to_database)) {
      HandleInitializationFailure();
      return false;
    }
  }

  table_manager_->InitializeOnDbSequence(
      db_.get(), std::vector<std::string>{kBudgetsTableName},
      kCurrentSchemaVersion);

  budgets_data_.InitializeOnDBSequence();

  return true;
}

void PrivateAggregationBudgetStorage::HandleInitializationFailure() {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());

  // TODO(crbug.com/1323320): Record histograms for the different
  // outcomes/errors.
  db_.reset();
}

void PrivateAggregationBudgetStorage::FinishInitializationOnMainSequence(
    std::unique_ptr<PrivateAggregationBudgetStorage> owned_this,
    base::OnceCallback<void(std::unique_ptr<PrivateAggregationBudgetStorage>)>
        on_done_initializing,
    bool was_successful) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(owned_this);

  // If the initialization failed, `this` will be destroyed after its unique_ptr
  // passes out of scope here.
  std::move(on_done_initializing)
      .Run(was_successful ? std::move(owned_this) : nullptr);
}

}  // namespace content
