// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webdata/common/web_database_backend.h"

#include <algorithm>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/webdata/common/web_data_request_manager.h"
#include "components/webdata/common/web_database.h"
#include "components/webdata/common/web_database_table.h"
#include "sql/error_delegate_util.h"
#include "sql/sqlite_result_code.h"

WebDatabaseBackend::WebDatabaseBackend(
    const base::FilePath& path,
    std::unique_ptr<Delegate> delegate,
    const scoped_refptr<base::SequencedTaskRunner>& db_thread)
    : base::RefCountedDeleteOnSequence<WebDatabaseBackend>(db_thread),
      db_path_(path),
      delegate_(std::move(delegate)) {
  // WebDatabaseBackend is created on the client sequence then accessed on the
  // DB sequence.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

void WebDatabaseBackend::AddTable(std::unique_ptr<WebDatabaseTable> table) {
  CHECK(!init_complete_) << "AddTable must be called before init starts.";
  tables_.push_back(std::move(table));
}

void WebDatabaseBackend::MaybeInitEncryptorOnUiSequence(
    os_crypt_async::Encryptor encryptor) {
  CHECK(!encryptor_) << "Init should only be called once.";
  // Encryptor is thread safe. This transfers it from the UI sequence to the DB
  // sequence. The CHECKs above and below guarantee this only happens once,
  // before any tasks have been posted to the DB sequence.
  encryptor_.emplace(std::move(encryptor));
}

void WebDatabaseBackend::InitDatabase() {
  // MaybeInitEncryptorOnUiSequence must be called first.
  CHECK(encryptor_);
  LoadDatabaseIfNecessary();
  if (delegate_)
    delegate_->DBLoaded(init_status_, diagnostics_);
}

void WebDatabaseBackend::ShutdownDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (db_ && init_status_ == sql::INIT_OK)
    db_->CommitTransaction();
  db_.reset();
  init_complete_ = true;  // Ensures the init sequence is not re-run.
  init_status_ = sql::INIT_FAILURE;
}

void WebDatabaseBackend::DBWriteTaskWrapper(
    WebDatabaseService::WriteTask task,
    std::unique_ptr<WebDataRequest> request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(init_complete_) << "Init must be complete before running a DB task.";
  if (!request->IsActive())
    return;
  ExecuteWriteTask(std::move(task));
  request_manager_->RequestCompleted(std::move(request), nullptr);
}

void WebDatabaseBackend::ExecuteWriteTask(WebDatabaseService::WriteTask task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(init_complete_) << "Init must be complete before running a DB task.";
  if (db_ && init_status_ == sql::INIT_OK) {
    WebDatabase::State state = std::move(task).Run(db_.get());
    if (state == WebDatabase::COMMIT_NEEDED)
      Commit();
  }
}

void WebDatabaseBackend::DBReadTaskWrapper(
    WebDatabaseService::ReadTask task,
    std::unique_ptr<WebDataRequest> request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(init_complete_) << "Init must be complete before running a DB task.";
  if (!request->IsActive())
    return;
  std::unique_ptr<WDTypedResult> result = ExecuteReadTask(std::move(task));
  request_manager_->RequestCompleted(std::move(request), std::move(result));
}

std::unique_ptr<WDTypedResult> WebDatabaseBackend::ExecuteReadTask(
    WebDatabaseService::ReadTask task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(init_complete_) << "Init must be complete before running a DB task.";
  return (db_ && init_status_ == sql::INIT_OK) ? std::move(task).Run(db_.get())
                                               : nullptr;
}

WebDatabaseBackend::~WebDatabaseBackend() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ShutdownDatabase();
}

void WebDatabaseBackend::LoadDatabaseIfNecessary() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (init_complete_ || db_path_.empty())
    return;

  init_complete_ = true;
  db_ = std::make_unique<WebDatabase>();

  for (const auto& table : tables_)
    db_->AddTable(table.get());

  // Unretained to avoid a ref loop since we own |db_|.
  db_->set_error_callback(base::BindRepeating(
      &WebDatabaseBackend::DatabaseErrorCallback, base::Unretained(this)));
  diagnostics_.clear();
  catastrophic_error_occurred_ = false;
  init_status_ = db_->Init(db_path_, &(*encryptor_));

  if (init_status_ != sql::INIT_OK) {
    db_.reset();
    return;
  }

  // A catastrophic error might have happened and recovered.
  if (catastrophic_error_occurred_)
    init_status_ = sql::INIT_OK_WITH_DATA_LOSS;
  db_->BeginTransaction();
}

void WebDatabaseBackend::DatabaseErrorCallback(int error,
                                               sql::Statement* statement) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sql::UmaHistogramSqliteResult("WebDatabase.DatabaseErrors", error);

  // We ignore any further error callbacks after the first catastrophic error.
  if (!catastrophic_error_occurred_ && sql::IsErrorCatastrophic(error)) {
    catastrophic_error_occurred_ = true;
    diagnostics_ = db_->GetDiagnosticInfo(error, statement);
    diagnostics_ += sql::GetCorruptFileDiagnosticsInfo(db_path_);

    db_->GetSQLConnection()->RazeAndPoison();
  }
}

void WebDatabaseBackend::Commit() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_);
  DCHECK_EQ(sql::INIT_OK, init_status_);
  db_->CommitTransaction();
  db_->BeginTransaction();
}
