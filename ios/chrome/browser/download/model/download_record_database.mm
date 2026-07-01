// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/download_record_database.h"

#import <string>

#import "base/functional/bind.h"
#import "base/i18n/string_search.h"
#import "base/no_destructor.h"
#import "base/notreached.h"
#import "base/strings/string_util.h"
#import "base/strings/stringprintf.h"
#import "base/strings/utf_string_conversions.h"
#import "base/time/time.h"
#import "ios/chrome/browser/download/model/download_filename_util.h"
#import "ios/chrome/browser/download/model/download_filter_util.h"
#import "ios/web/public/download/download_task.h"
#import "sql/error_delegate_util.h"
#import "sql/meta_table.h"
#import "sql/statement.h"
#import "sql/transaction.h"

namespace {

using ::download_model::NormalizeFileName;

// Schema version.
// v1: initial schema.
// v2: add `idx_records_created_time` composite index and
//     `file_name_normalized` column (case-folded `file_name`) backfilled for
//     existing rows, to support keyset pagination + case-insensitive
//     substring search.
const int kCurrentVersionNumber = 2;
// Lowest version that can still read v2. v1 binaries cannot — the
// insert/update statements assume `file_name_normalized` exists.
const int kCompatibleVersionNumber = 2;

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
const char kFileNameNormalizedField[] = "file_name_normalized";

const char kTableName[] = "download_records";
const char kIndexName[] = "idx_records_created_time";

const std::string& CreateTableSql() {
  static const base::NoDestructor<std::string> sql(base::StringPrintf(
      "CREATE TABLE IF NOT EXISTS %s ("
      "%s TEXT PRIMARY KEY NOT NULL,%s TEXT NOT NULL,%s TEXT,%s TEXT NOT NULL,"
      "%s TEXT,%s TEXT,%s TEXT,%s TEXT,%s TEXT,%s TEXT,%s TEXT,"
      "%s INTEGER,%s INTEGER,%s INTEGER,%s INTEGER,%s INTEGER,%s INTEGER,%s "
      "INTEGER,%s TEXT)",
      kTableName, kDownloadIdField, kOriginalUrlField, kRedirectedUrlField,
      kFileNameField, kFilePathField, kResponsePathField,
      kOriginalMimeTypeField, kMimeTypeField, kContentDispositionField,
      kOriginatingHostField, kHttpMethodField, kHttpCodeField, kErrorCodeField,
      kTotalBytesField, kStateField, kCreatedTimeField, kCompletedTimeField,
      kHasPerformedBackgroundDownloadField, kFileNameNormalizedField));
  return *sql;
}

const std::string& InsertRecordSql() {
  static const base::NoDestructor<std::string> sql(base::StringPrintf(
      "INSERT INTO %s "
      "(%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s) "
      "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
      kTableName, kDownloadIdField, kOriginalUrlField, kRedirectedUrlField,
      kFileNameField, kFilePathField, kResponsePathField,
      kOriginalMimeTypeField, kMimeTypeField, kContentDispositionField,
      kOriginatingHostField, kHttpMethodField, kHttpCodeField, kErrorCodeField,
      kTotalBytesField, kStateField, kCreatedTimeField, kCompletedTimeField,
      kHasPerformedBackgroundDownloadField, kFileNameNormalizedField));
  return *sql;
}

const std::string& UpdateRecordSql() {
  static const base::NoDestructor<std::string> sql(base::StringPrintf(
      "UPDATE %s SET %s=?,%s=?,%s=?,%s=?,%s=?,%s=?,%s=?,%s=?,%s=?,%s=?,"
      "%s=?,%s=?,%s=?,%s=?,%s=?,%s=?,%s=? WHERE %s=?",
      kTableName, kOriginalUrlField, kRedirectedUrlField, kFileNameField,
      kFilePathField, kResponsePathField, kOriginalMimeTypeField,
      kMimeTypeField, kContentDispositionField, kOriginatingHostField,
      kHttpMethodField, kHttpCodeField, kErrorCodeField, kTotalBytesField,
      kStateField, kCompletedTimeField, kHasPerformedBackgroundDownloadField,
      kFileNameNormalizedField, kDownloadIdField));
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

// Composite index supporting keyset pagination ordered by
// `(created_time DESC, download_id DESC)`. Both columns `DESC` lets the
// pagination `ORDER BY` be satisfied without a separate sort.
const std::string& CreateIndexSql() {
  static const base::NoDestructor<std::string> sql(base::StringPrintf(
      "CREATE INDEX IF NOT EXISTS %s ON %s (%s DESC, %s DESC)", kIndexName,
      kTableName, kCreatedTimeField, kDownloadIdField));
  return *sql;
}

// Column list shared by row-returning `SELECT` statements. Order matches
// `CreateRecordFromStatement`.
const std::string& SelectColumns() {
  static const base::NoDestructor<std::string> cols(base::StringPrintf(
      "%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s", kDownloadIdField,
      kOriginalUrlField, kRedirectedUrlField, kFileNameField, kFilePathField,
      kResponsePathField, kOriginalMimeTypeField, kMimeTypeField,
      kContentDispositionField, kOriginatingHostField, kHttpMethodField,
      kHttpCodeField, kErrorCodeField, kTotalBytesField, kStateField,
      kCreatedTimeField, kCompletedTimeField,
      kHasPerformedBackgroundDownloadField));
  return *cols;
}

// Single SQL `UPDATE` that flips state from `kNotStarted` or `kInProgress`
// to `kFailed`. Other states are untouched.
const std::string& MarkUnfinishedAsFailedSql() {
  static const base::NoDestructor<std::string> sql(
      base::StringPrintf("UPDATE %s SET %s=? WHERE %s IN (?, ?)", kTableName,
                         kStateField, kStateField));
  return *sql;
}

// SQL predicate matching `filter_type` against `mime_type`. Returns "1" for
// `kAll`/unset so it composes cleanly inside a `WHERE` clause. Categories
// mirror `download_filter_util.cc` (PDF = exact `application/pdf`;
// Video/Audio/Image/Document = prefix match; Other = none of the above).
std::string FilterClauseForType(
    const std::optional<DownloadFilterType>& filter_type) {
  if (!filter_type.has_value()) {
    return "1";
  }
  switch (filter_type.value()) {
    case DownloadFilterType::kPDF:
      return base::StringPrintf("LOWER(%s) = 'application/pdf'",
                                kMimeTypeField);
    case DownloadFilterType::kImage:
      return base::StringPrintf("LOWER(%s) LIKE 'image/%%'", kMimeTypeField);
    case DownloadFilterType::kVideo:
      return base::StringPrintf("LOWER(%s) LIKE 'video/%%'", kMimeTypeField);
    case DownloadFilterType::kAudio:
      return base::StringPrintf("LOWER(%s) LIKE 'audio/%%'", kMimeTypeField);
    case DownloadFilterType::kDocument:
      return base::StringPrintf("LOWER(%s) LIKE 'text/%%'", kMimeTypeField);
    case DownloadFilterType::kOther:
      // Anything not PDF and not in the prefix categories above.
      return base::StringPrintf("LOWER(%s) <> 'application/pdf' AND "
                                "LOWER(%s) NOT LIKE 'image/%%' AND "
                                "LOWER(%s) NOT LIKE 'video/%%' AND "
                                "LOWER(%s) NOT LIKE 'audio/%%' AND "
                                "LOWER(%s) NOT LIKE 'text/%%'",
                                kMimeTypeField, kMimeTypeField, kMimeTypeField,
                                kMimeTypeField, kMimeTypeField);
    case DownloadFilterType::kAll:
      // Reached only when `filter_type` is explicitly `kAll`; the
      // `!has_value()` path above handles the unset case. Kept here for
      // the exhaustive switch.
      return "1";
  }
  NOTREACHED();
}

// Wraps `query` with leading/trailing '%' and escapes `LIKE` wildcards
// ('%', '_', '\\') with a leading '\\'. The corresponding SQL must use
// `ESCAPE '\\'`. `query` must already be `NormalizeFileName`-ed.
std::string BuildLikePattern(std::string_view query) {
  std::string escaped;
  escaped.reserve(query.size() + 2);
  escaped.push_back('%');
  for (char c : query) {
    if (c == '%' || c == '_' || c == '\\') {
      escaped.push_back('\\');
    }
    escaped.push_back(c);
  }
  escaped.push_back('%');
  return escaped;
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

  if (db_.is_open()) {
    return init_status_;
  }

  db_.set_error_callback(base::BindRepeating(
      &DownloadRecordDatabase::DatabaseErrorCallback, base::Unretained(this)));

  if (!db_.Open(db_path_)) {
    init_status_ = sql::INIT_FAILURE;
    return init_status_;
  }

  if (!meta_table_->Init(&db_, kCurrentVersionNumber,
                         kCompatibleVersionNumber)) {
    init_status_ = sql::INIT_FAILURE;
    return init_status_;
  }

  if (meta_table_->GetCompatibleVersionNumber() > kCurrentVersionNumber) {
    init_status_ = sql::INIT_TOO_NEW;
    return init_status_;
  }

  if (!UpgradeDatabase()) {
    init_status_ = sql::INIT_FAILURE;
    return init_status_;
  }

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

  // Incognito records are in-memory only at the service layer and must never
  // reach the DB. Release-build guard against upstream bugs: a privacy leak
  // is worse than dropping a record.
  if (record.is_incognito) {
    return false;
  }

  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE, InsertRecordSql()));
  BindRecordToInsertStatement(statement, record);

  return statement.Run();
}

bool DownloadRecordDatabase::UpdateDownloadRecord(
    const DownloadRecord& record) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!db_.is_open()) {
    return false;
  }

  // See `InsertDownloadRecord` — incognito records must never reach the DB.
  if (record.is_incognito) {
    return false;
  }

  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE, UpdateRecordSql()));

  BindRecordToUpdateStatement(statement, record);

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
    records.push_back(CreateRecordFromStatement(statement));
  }

  return records;
}

std::vector<DownloadRecord> DownloadRecordDatabase::GetDownloadRecordsPage(
    const DownloadRecordQuery& query) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<DownloadRecord> records;

  if (!db_.is_open()) {
    return records;
  }

  const bool has_name_query =
      query.name_query.has_value() && !query.name_query.value().empty();

  // Two-phase name filter:
  //   Phase 1 (SQL): `LIKE` on the normalized column cheaply prunes the
  //                  candidate set via the composite index.
  //   Phase 2 (C++): re-check each candidate with the ICU-backed
  //                  `FixedPatternStringSearchIgnoringCaseAndAccents`
  //                  on raw `file_name`. ICU is the source of truth, so
  //                  results stay aligned with the in-memory search API even
  //                  if the NFD+stripMn+FoldCase approximation drifts.
  // We batch-read until we collect `kPageSize` matches or the SQL stream is
  // exhausted. The cursor advances to the last scanned row (not the last
  // kept row), so the next page continues strictly past the deepest scanned
  // position without revisiting candidates.
  std::unique_ptr<base::i18n::FixedPatternStringSearchIgnoringCaseAndAccents>
      icu_search;
  if (has_name_query) {
    icu_search = std::make_unique<
        base::i18n::FixedPatternStringSearchIgnoringCaseAndAccents>(
        base::UTF8ToUTF16(query.name_query.value()));
  }

  std::optional<base::Time> cursor_time = query.cursor_created_time;
  std::optional<std::string> cursor_id = query.cursor_download_id;

  // Cap iterations to avoid pathological loops if every batch fails the
  // ICU re-check. Generous in practice: phase-1 NFD+stripMn+FoldCase is a
  // tight superset of phase-2 ICU primary collation.
  const int kMaxBatches = 32;
  for (int batch = 0; batch < kMaxBatches; ++batch) {
    const bool has_cursor = cursor_time.has_value() && cursor_id.has_value();
    std::string cursor_clause =
        has_cursor ? base::StringPrintf("AND (%s < ? OR (%s = ? AND %s < ?))",
                                        kCreatedTimeField, kCreatedTimeField,
                                        kDownloadIdField)
                   : std::string();
    std::string name_clause =
        has_name_query ? base::StringPrintf("AND %s LIKE ? ESCAPE '\\'",
                                            kFileNameNormalizedField)
                       : std::string();

    // Build the `WHERE` clause. Cursor predicate uses the expanded `OR`
    // form (rather than SQLite tuple comparison `(a, b) < (?, ?)`) because
    // SQLite is more reliable about using the composite index that way.
    // `ORDER BY` mirrors the index (`created_time DESC, download_id DESC`),
    // so `EXPLAIN QUERY PLAN` reports
    // `USING INDEX idx_records_created_time`.
    std::string sql = base::StringPrintf(
        "SELECT %s FROM %s WHERE (%s) %s %s "
        "ORDER BY %s DESC, %s DESC LIMIT ?",
        SelectColumns().c_str(), kTableName,
        FilterClauseForType(query.filter_type).c_str(), name_clause.c_str(),
        cursor_clause.c_str(), kCreatedTimeField, kDownloadIdField);

    sql::Statement statement(db_.GetUniqueStatement(sql));

    int param_index = 0;
    if (has_name_query) {
      // Escape `LIKE` wildcards in user input so e.g. "100%" is literal.
      statement.BindString(
          param_index++,
          BuildLikePattern(NormalizeFileName(query.name_query.value())));
    }
    if (has_cursor) {
      int64_t cursor_micros =
          cursor_time.value().ToDeltaSinceWindowsEpoch().InMicroseconds();
      statement.BindInt64(param_index++, cursor_micros);
      statement.BindInt64(param_index++, cursor_micros);
      statement.BindString(param_index++, cursor_id.value());
    }
    // Over-fetch when ICU re-check might prune rows; otherwise grab exactly
    // what's needed.
    int batch_limit =
        has_name_query
            ? std::max<int>(kDownloadRecordsPageSize * 2,
                            kDownloadRecordsPageSize -
                                static_cast<int>(records.size()) + 4)
            : (kDownloadRecordsPageSize - static_cast<int>(records.size()));
    if (batch_limit <= 0) {
      break;
    }
    statement.BindInt(param_index++, batch_limit);

    int rows_seen_this_batch = 0;
    std::optional<base::Time> last_row_time;
    std::optional<std::string> last_row_id;
    while (statement.Step()) {
      DownloadRecord record = CreateRecordFromStatement(statement);
      ++rows_seen_this_batch;
      last_row_time = record.created_time;
      last_row_id = record.download_id;

      bool keep = true;
      if (icu_search) {
        size_t match_index = 0;
        size_t match_length = 0;
        keep = icu_search->Search(base::UTF8ToUTF16(record.file_name),
                                  &match_index, &match_length);
      }
      if (keep) {
        records.push_back(std::move(record));
        if (static_cast<int>(records.size()) >= kDownloadRecordsPageSize) {
          break;
        }
      }
    }

    if (static_cast<int>(records.size()) >= kDownloadRecordsPageSize) {
      break;
    }
    // SQL stream exhausted for this filter; no more pages.
    if (rows_seen_this_batch < batch_limit) {
      break;
    }
    // Advance cursor and loop for the next batch.
    cursor_time = last_row_time;
    cursor_id = last_row_id;
  }

  return records;
}

int DownloadRecordDatabase::GetDownloadRecordsCount(
    const DownloadRecordQuery& query) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!db_.is_open()) {
    return 0;
  }

  const bool has_name_query =
      query.name_query.has_value() && !query.name_query.value().empty();
  std::string name_clause =
      has_name_query ? base::StringPrintf("AND %s LIKE ? ESCAPE '\\'",
                                          kFileNameNormalizedField)
                     : std::string();

  if (!has_name_query) {
    // Fast path: pure SQL `COUNT(*)`.
    std::string sql =
        base::StringPrintf("SELECT COUNT(*) FROM %s WHERE (%s)", kTableName,
                           FilterClauseForType(query.filter_type).c_str());
    sql::Statement statement(db_.GetUniqueStatement(sql));
    if (!statement.Step()) {
      return 0;
    }
    return statement.ColumnInt(0);
  }

  // Two-phase count: SQL `LIKE` pre-filter + ICU re-check in C++. Same
  // semantics guarantee as `GetDownloadRecordsPage`.
  std::string sql = base::StringPrintf(
      "SELECT %s FROM %s WHERE (%s) %s", kFileNameField, kTableName,
      FilterClauseForType(query.filter_type).c_str(), name_clause.c_str());
  sql::Statement statement(db_.GetUniqueStatement(sql));
  statement.BindString(
      0, BuildLikePattern(NormalizeFileName(query.name_query.value())));

  base::i18n::FixedPatternStringSearchIgnoringCaseAndAccents icu_search(
      base::UTF8ToUTF16(query.name_query.value()));
  int count = 0;
  while (statement.Step()) {
    size_t match_index = 0;
    size_t match_length = 0;
    if (icu_search.Search(base::UTF8ToUTF16(statement.ColumnString(0)),
                          &match_index, &match_length)) {
      ++count;
    }
  }
  return count;
}

bool DownloadRecordDatabase::MarkUnfinishedDownloadsAsFailed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!db_.is_open()) {
    return false;
  }

  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE, MarkUnfinishedAsFailedSql()));
  statement.BindInt(0, static_cast<int>(web::DownloadTask::State::kFailed));
  statement.BindInt(1, static_cast<int>(web::DownloadTask::State::kNotStarted));
  statement.BindInt(2, static_cast<int>(web::DownloadTask::State::kInProgress));

  return statement.Run();
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

  // Composite index used by `GetDownloadRecordsPage` to satisfy the
  // `ORDER BY (created_time DESC, download_id DESC)` without an extra sort.
  if (!db_.Execute(CreateIndexSql())) {
    return false;
  }

  return transaction.Commit();
}

bool DownloadRecordDatabase::UpgradeDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  int current_version = meta_table_->GetVersionNumber();

  if (current_version == kCurrentVersionNumber) {
    return true;
  }

  // v1 -> v2: add `file_name_normalized` column (backfilled from `file_name`)
  // and create the composite index for keyset pagination.
  if (current_version == 1) {
    sql::Transaction transaction(&db_);
    if (!transaction.Begin()) {
      return false;
    }
    // `ALTER TABLE ADD COLUMN` with a NULL default; backfill below.
    std::string add_col =
        base::StringPrintf("ALTER TABLE %s ADD COLUMN %s TEXT", kTableName,
                           kFileNameNormalizedField);
    if (!db_.Execute(add_col)) {
      return false;
    }
    // Backfill: walk every row and compute normalized `file_name` in C++.
    // Collect `(id, normalized)` first to avoid interleaving `SELECT` with
    // `UPDATE` on the same connection, then push updates through a single
    // reused statement.
    std::string select_all = base::StringPrintf(
        "SELECT %s, %s FROM %s", kDownloadIdField, kFileNameField, kTableName);
    std::vector<std::pair<std::string, std::string>> updates;
    {
      sql::Statement reader(db_.GetUniqueStatement(select_all));
      while (reader.Step()) {
        updates.emplace_back(reader.ColumnString(0),
                             NormalizeFileName(reader.ColumnString(1)));
      }
    }
    std::string update_one =
        base::StringPrintf("UPDATE %s SET %s=? WHERE %s=?", kTableName,
                           kFileNameNormalizedField, kDownloadIdField);
    sql::Statement writer(db_.GetUniqueStatement(update_one));
    for (const auto& [id, normalized] : updates) {
      writer.Reset(/*clear_bound_vars=*/true);
      writer.BindString(0, normalized);
      writer.BindString(1, id);
      if (!writer.Run()) {
        return false;
      }
    }
    if (!db_.Execute(CreateIndexSql())) {
      return false;
    }
    if (!transaction.Commit()) {
      return false;
    }
    return meta_table_->SetVersionNumber(kCurrentVersionNumber) &&
           meta_table_->SetCompatibleVersionNumber(kCompatibleVersionNumber);
  }

  return true;
}

bool DownloadRecordDatabase::DoesTableExist(const std::string& table_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  sql::Statement statement(db_.GetUniqueStatement(CheckTableExistsSql()));
  statement.BindString(0, table_name);

  return statement.Step();
}

void DownloadRecordDatabase::BindRecordToInsertStatement(
    sql::Statement& statement,
    const DownloadRecord& record) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Parameter order matches `InsertRecordSql`.
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
  statement.BindString(18, NormalizeFileName(record.file_name));
}

void DownloadRecordDatabase::BindRecordToUpdateStatement(
    sql::Statement& statement,
    const DownloadRecord& record) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Parameter order matches `UpdateRecordSql`. `created_time` is
  // intentionally excluded — it must not change after insert.
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
  statement.BindString(16, NormalizeFileName(record.file_name));
  statement.BindString(17, record.download_id);
}

DownloadRecord DownloadRecordDatabase::CreateRecordFromStatement(
    sql::Statement& statement) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DownloadRecord record;

  // Column order matches `SelectRecordSql` and `SelectAllRecordsSql`.
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
