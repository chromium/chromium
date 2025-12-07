// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/download_record_database.h"

#import <string>

#import "base/functional/bind.h"
#import "base/no_destructor.h"
#import "base/strings/stringprintf.h"
#import "base/time/time.h"
#import "ios/web/public/download/download_task.h"
#import "sql/error_delegate_util.h"
#import "sql/meta_table.h"
#import "sql/statement.h"
#import "sql/transaction.h"

namespace {

// Database schema version.
const int kCurrentVersionNumber = 1;
const int kCompatibleVersionNumber = 1;

// Field name constants.
const char kDownloadIdField[] = "download_id";
const char kOriginalUrlField[] = "original_url";
const char kRedirectedUrlField[] = "redirected_url";
const char kFileNameField[] = "file_name";
const char kFilePathField[] = "file_path";
const char kResponsePathField[] = "response_path";
const char kOriginalMimeTypeField[] = "original_mime_type";
const char kMimeTypeField[] = "mime_type";
const char kContentDispositionField[] = "content_disposition";
const char kOriginatingHostField[] = "originating_host";
const char kHttpMethodField[] = "http_method";
const char kHttpCodeField[] = "http_code";
const char kErrorCodeField[] = "error_code";
const char kTotalBytesField[] = "total_bytes";
const char kStateField[] = "state";
const char kCreatedTimeField[] = "created_time";
const char kCompletedTimeField[] = "completed_time";
const char kHasPerformedBackgroundDownloadField[] =
    "has_performed_background_download";

const char kTableName[] = "download_records";

const std::string& CreateTableSql() {
  static const base::NoDestructor<std::string> sql(base::StringPrintf(
      "CREATE TABLE IF NOT EXISTS %s ("
      "%s TEXT PRIMARY KEY NOT NULL,%s TEXT NOT NULL,%s TEXT,%s TEXT NOT NULL,"
      "%s TEXT,%s TEXT,%s TEXT,%s TEXT,%s TEXT,%s TEXT,%s TEXT,"
      "%s INTEGER,%s INTEGER,%s INTEGER,%s INTEGER,%s INTEGER,%s INTEGER,%s "
      "INTEGER)",
      kTableName, kDownloadIdField, kOriginalUrlField, kRedirectedUrlField,
      kFileNameField, kFilePathField, kResponsePathField,
      kOriginalMimeTypeField, kMimeTypeField, kContentDispositionField,
      kOriginatingHostField, kHttpMethodField, kHttpCodeField, kErrorCodeField,
      kTotalBytesField, kStateField, kCreatedTimeField, kCompletedTimeField,
      kHasPerformedBackgroundDownloadField));
  return *sql;
}

const std::string& InsertRecordSql() {
  static const base::NoDestructor<std::string> sql(base::StringPrintf(
      "INSERT INTO %s (%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s) "
      "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
      kTableName, kDownloadIdField, kOriginalUrlField, kRedirectedUrlField,
      kFileNameField, kFilePathField, kResponsePathField,
      kOriginalMimeTypeField, kMimeTypeField, kContentDispositionField,
      kOriginatingHostField, kHttpMethodField, kHttpCodeField, kErrorCodeField,
      kTotalBytesField, kStateField, kCreatedTimeField, kCompletedTimeField,
      kHasPerformedBackgroundDownloadField));
  return *sql;
}

const std::string& UpdateRecordSql() {
  static const base::NoDestructor<std::string> sql(base::StringPrintf(
      "UPDATE %s SET %s=?,%s=?,%s=?,%s=?,%s=?,%s=?,%s=?,%s=?,%s=?,%s=?,"
      "%s=?,%s=?,%s=?,%s=?,%s=?,%s=? WHERE %s=?",
      kTableName, kOriginalUrlField, kRedirectedUrlField, kFileNameField,
      kFilePathField, kResponsePathField, kOriginalMimeTypeField,
      kMimeTypeField, kContentDispositionField, kOriginatingHostField,
      kHttpMethodField, kHttpCodeField, kErrorCodeField, kTotalBytesField,
      kStateField, kCompletedTimeField, kHasPerformedBackgroundDownloadField,
      kDownloadIdField));
  return *sql;
}

const std::string& SelectRecordSql() {
  static const base::NoDestructor<std::string> sql(base::StringPrintf(
      "SELECT %s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s "
      "FROM %s WHERE %s=?",
      kDownloadIdField, kOriginalUrlField, kRedirectedUrlField, kFileNameField,
      kFilePathField, kResponsePathField, kOriginalMimeTypeField,
      kMimeTypeField, kContentDispositionField, kOriginatingHostField,
      kHttpMethodField, kHttpCodeField, kErrorCodeField, kTotalBytesField,
      kStateField, kCreatedTimeField, kCompletedTimeField,
      kHasPerformedBackgroundDownloadField, kTableName, kDownloadIdField));
  return *sql;
}

const std::string& SelectAllRecordsSql() {
  static const base::NoDestructor<std::string> sql(base::StringPrintf(
      "SELECT %s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s "
      "FROM %s ORDER BY %s DESC",
      kDownloadIdField, kOriginalUrlField, kRedirectedUrlField, kFileNameField,
      kFilePathField, kResponsePathField, kOriginalMimeTypeField,
      kMimeTypeField, kContentDispositionField, kOriginatingHostField,
      kHttpMethodField, kHttpCodeField, kErrorCodeField, kTotalBytesField,
      kStateField, kCreatedTimeField, kCompletedTimeField,
      kHasPerformedBackgroundDownloadField, kTableName, kCreatedTimeField));
  return *sql;
}

const std::string& DeleteRecordSql() {
  static const base::NoDestructor<std::string> sql(base::StringPrintf(
      "DELETE FROM %s WHERE %s=?", kTableName, kDownloadIdField));
  return *sql;
}

const std::string& UpdateRecordsStateSql() {
  static const base::NoDestructor<std::string> sql(
      base::StringPrintf("UPDATE %s SET %s=? WHERE %s=?", kTableName,
                         kStateField, kDownloadIdField));
  return *sql;
}

const std::string& CheckTableExistsSql() {
  static const base::NoDestructor<std::string> sql(
      "SELECT name FROM sqlite_master WHERE type='table' AND name=?");
  return *sql;
}

}  // namespace

DownloadRecordDatabase::DownloadRecordDatabase(const base::FilePath& db_path)
    : db_path_(db_path),
      db_(sql::Database::Tag("DownloadRecord")),
      meta_table_(std::make_unique<sql::MetaTable>()) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

DownloadRecordDatabase::~DownloadRecordDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

sql::InitStatus DownloadRecordDatabase::Init() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If already open, nothing to do.
  if (db_.is_open()) {
    return init_status_;  // Return the previous initialization status
  }

  // Set up error callback.
  db_.set_error_callback(base::BindRepeating(
      &DownloadRecordDatabase::DatabaseErrorCallback, base::Unretained(this)));

  // Open the database.
  if (!db_.Open(db_path_)) {
    init_status_ = sql::INIT_FAILURE;
    return init_status_;
  }

  // Initialize meta table.
  if (!meta_table_->Init(&db_, kCurrentVersionNumber,
                         kCompatibleVersionNumber)) {
    init_status_ = sql::INIT_FAILURE;
    return init_status_;
  }

  // Check version compatibility.
  if (meta_table_->GetCompatibleVersionNumber() > kCurrentVersionNumber) {
    init_status_ = sql::INIT_TOO_NEW;
    return init_status_;
  }

  // Upgrade database if necessary.
  if (!UpgradeDatabase()) {
    init_status_ = sql::INIT_FAILURE;
    return init_status_;
  }

  // Create schema if needed.
  if (!CreateSchema()) {
    init_status_ = sql::INIT_FAILURE;
    return init_status_;
  }

  init_status_ = sql::INIT_OK;
  return init_status_;
}

bool DownloadRecordDatabase::InsertDownloadRecord(
    const DownloadRecord& record) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!db_.is_open()) {
    return false;
  }

  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE, InsertRecordSql()));
  if (!BindRecordToInsertStatement(statement, record)) {
    return false;
  }

  return statement.Run();
}

bool DownloadRecordDatabase::UpdateDownloadRecord(
    const DownloadRecord& record) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!db_.is_open()) {
    return false;
  }

  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE, UpdateRecordSql()));

  if (!BindRecordToUpdateStatement(statement, record)) {
    return false;
  }

  return statement.Run();
}

bool DownloadRecordDatabase::UpdateDownloadRecordsState(
    const std::vector<std::string>& download_ids,
    web::DownloadTask::State new_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!db_.is_open() || download_ids.empty()) {
    return false;
  }

  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return false;
  }

  sql::Statement statement(db_.GetUniqueStatement(UpdateRecordsStateSql()));

  for (const std::string& download_id : download_ids) {
    statement.Reset(true);
    statement.BindInt(0, static_cast<int>(new_state));
    statement.BindString(1, download_id);

    if (!statement.Run()) {
      return false;
    }
  }

  return transaction.Commit();
}

bool DownloadRecordDatabase::DeleteDownloadRecord(
    const std::string& download_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!db_.is_open()) {
    return false;
  }

  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE, DeleteRecordSql()));
  statement.BindString(0, download_id);

  return statement.Run();
}

std::optional<DownloadRecord> DownloadRecordDatabase::GetDownloadRecord(
    const std::string& download_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!db_.is_open()) {
    return std::nullopt;
  }

  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE, SelectRecordSql()));
  statement.BindString(0, download_id);

  if (!statement.Step()) {
    return std::nullopt;
  }

  return CreateRecordFromStatement(statement);
}

std::vector<DownloadRecord> DownloadRecordDatabase::GetAllDownloadRecords() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<DownloadRecord> records;

  if (!db_.is_open()) {
    return records;
  }

  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE, SelectAllRecordsSql()));

  while (statement.Step()) {
    auto record = CreateRecordFromStatement(statement);
    if (record) {
      records.push_back(std::move(*record));
    }
  }

  return records;
}

bool DownloadRecordDatabase::IsInitialized() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return init_status_ == sql::INIT_OK;
}

bool DownloadRecordDatabase::CreateSchema() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return false;
  }

  if (!db_.Execute(CreateTableSql())) {
    return false;
  }

  return transaction.Commit();
}

bool DownloadRecordDatabase::UpgradeDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  int current_version = meta_table_->GetVersionNumber();

  // No upgrade needed if already at current version.
  if (current_version == kCurrentVersionNumber) {
    return true;
  }

  // Future version upgrade logic goes here.
  // Example for upgrading from version 1 to 2:
  // if (current_version == 1 && kCurrentVersionNumber >= 2) {
  //   if (!UpgradeToVersion2()) {
  //     return false;
  //   }
  // }

  // Update meta table with new version.
  return meta_table_->SetVersionNumber(kCurrentVersionNumber);
}

bool DownloadRecordDatabase::DoesTableExist(const std::string& table_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  sql::Statement statement(db_.GetUniqueStatement(CheckTableExistsSql()));
  statement.BindString(0, table_name);

  return statement.Step();
}

bool DownloadRecordDatabase::BindRecordToInsertStatement(
    sql::Statement& statement,
    const DownloadRecord& record) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Parameter order matches InsertRecordSql().
  statement.BindString(0, record.download_id);
  statement.BindString(1, record.original_url);
  statement.BindString(2, record.redirected_url);
  statement.BindString(3, record.file_name);
  statement.BindString(4, record.file_path.value());
  statement.BindString(5, record.response_path.value());
  statement.BindString(6, record.original_mime_type);
  statement.BindString(7, record.mime_type);
  statement.BindString(8, record.content_disposition);
  statement.BindString(9, record.originating_host);
  statement.BindString(10, record.http_method);
  statement.BindInt(11, record.http_code);
  statement.BindInt(12, record.error_code);
  statement.BindInt64(13, record.total_bytes);
  statement.BindInt(14, static_cast<int>(record.state));
  statement.BindInt64(
      15, record.created_time.ToDeltaSinceWindowsEpoch().InMicroseconds());

  int64_t completed_time_microseconds = 0;
  if (!record.completed_time.is_null()) {
    completed_time_microseconds =
        record.completed_time.ToDeltaSinceWindowsEpoch().InMicroseconds();
  }
  statement.BindInt64(16, completed_time_microseconds);
  statement.BindBool(17, record.has_performed_background_download);

  return true;
}

bool DownloadRecordDatabase::BindRecordToUpdateStatement(
    sql::Statement& statement,
    const DownloadRecord& record) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Parameter order matches UpdateRecordSql().
  // Note: created_time is intentionally excluded - it should not be updated.
  statement.BindString(0, record.original_url);
  statement.BindString(1, record.redirected_url);
  statement.BindString(2, record.file_name);
  statement.BindString(3, record.file_path.value());
  statement.BindString(4, record.response_path.value());
  statement.BindString(5, record.original_mime_type);
  statement.BindString(6, record.mime_type);
  statement.BindString(7, record.content_disposition);
  statement.BindString(8, record.originating_host);
  statement.BindString(9, record.http_method);
  statement.BindInt(10, record.http_code);
  statement.BindInt(11, record.error_code);
  statement.BindInt64(12, record.total_bytes);
  statement.BindInt(13, static_cast<int>(record.state));

  int64_t completed_time_microseconds = 0;
  if (!record.completed_time.is_null()) {
    completed_time_microseconds =
        record.completed_time.ToDeltaSinceWindowsEpoch().InMicroseconds();
  }
  statement.BindInt64(14, completed_time_microseconds);
  statement.BindBool(15, record.has_performed_background_download);
  statement.BindString(16, record.download_id);

  return true;
}

std::optional<DownloadRecord> DownloadRecordDatabase::CreateRecordFromStatement(
    sql::Statement& statement) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DownloadRecord record;

  // Column order matches SelectRecordSql() and SelectAllRecordsSql().
  record.download_id = statement.ColumnString(0);
  record.original_url = statement.ColumnString(1);
  record.redirected_url = statement.ColumnString(2);
  record.file_name = statement.ColumnString(3);
  record.file_path = base::FilePath(statement.ColumnString(4));
  record.response_path = base::FilePath(statement.ColumnString(5));
  record.original_mime_type = statement.ColumnString(6);
  record.mime_type = statement.ColumnString(7);
  record.content_disposition = statement.ColumnString(8);
  record.originating_host = statement.ColumnString(9);
  record.http_method = statement.ColumnString(10);
  record.http_code = statement.ColumnInt(11);
  record.error_code = statement.ColumnInt(12);
  record.total_bytes = statement.ColumnInt64(13);
  record.state = static_cast<web::DownloadTask::State>(statement.ColumnInt(14));
  record.created_time = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(statement.ColumnInt64(15)));

  int64_t completed_time_microseconds = statement.ColumnInt64(16);
  if (completed_time_microseconds > 0) {
    record.completed_time = base::Time::FromDeltaSinceWindowsEpoch(
        base::Microseconds(completed_time_microseconds));
  }
  record.has_performed_background_download = statement.ColumnBool(17);

  return record;
}

void DownloadRecordDatabase::DatabaseErrorCallback(int error,
                                                   sql::Statement* stmt) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (sql::IsErrorCatastrophic(error)) {
    db_.Poison();
  }
}
