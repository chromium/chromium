// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/trace_report_database.h"

#include <optional>
#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/token.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"

namespace content {

namespace {

const base::FilePath::CharType kLocalTracesDatabasePath[] =
    FILE_PATH_LITERAL("LocalTraces.db");
const char kLocalTracesIndexTableName[] = "local_traces_index";
const char kLocalTracesPayloadsTableName[] = "local_traces_payloads";
constexpr int kCurrentVersionNumber = 6;

ClientTraceReport GetReportFromStatement(sql::Statement& statement) {
  auto trace_id = base::Token::FromString(statement.ColumnStringView(0));
  CHECK(trace_id.has_value());

  ClientTraceReport client_report;
  client_report.uuid = *trace_id;
  client_report.creation_time = statement.ColumnTime(1);
  client_report.scenario_name = statement.ColumnString(2);
  client_report.upload_rule_name = statement.ColumnString(3);
  if (statement.GetColumnType(4) != sql::ColumnType::kNull) {
    client_report.upload_rule_value = statement.ColumnInt(4);
  }

  client_report.upload_state =
      static_cast<ReportUploadState>(statement.ColumnInt(5));
  client_report.upload_time = statement.ColumnTime(6);
  client_report.skip_reason =
      static_cast<SkipUploadReason>(statement.ColumnInt(7));
  client_report.total_size = static_cast<uint64_t>(statement.ColumnInt64(8));
  client_report.has_trace_content = statement.ColumnBool(9);

  return client_report;
}

// create table `local_traces_index` with following columns:
// `uuid` is the unique ID of the trace.
// `creation_time` The date and time in seconds when the row was created.
// `scenario_name` The trace scenario name.
// `upload_rule_name` The name of the rule that triggered the upload.
// `upload_rule_value` The value of the rule that triggered the upload.
// `state` The current upload state of the trace.
// `upload_time` Time at which the trace was uploaded. NULL if not uploaded.
// `skip_reason` Reason why a trace was not uploaded.
// `file_size` The size of trace in bytes.
constexpr char kLocalTracesIndexTableSql[] =
    // clang-format off
  "CREATE TABLE IF NOT EXISTS local_traces_index("
    "uuid TEXT PRIMARY KEY NOT NULL,"
    "creation_time DATETIME NOT NULL,"
    "scenario_name TEXT NOT NULL,"
    "upload_rule_name TEXT NOT NULL,"
    "upload_rule_value INT NULL,"
    "state INT NOT NULL,"
    "upload_time DATETIME NULL,"
    "skip_reason INT NOT NULL,"
    "file_size INTEGER NOT NULL)";
// clang-format on

// create table `local_traces_payloads` with following columns:
// `uuid` is the unique ID of the trace.
// `trace_content` The serialized trace content string
// `system_profile` The serialized system profile string
constexpr char kLocalTracesPayloadsTableSql[] =
    // clang-format off
  "CREATE TABLE IF NOT EXISTS local_traces_payloads("
    "uuid TEXT PRIMARY KEY NOT NULL,"
    "trace_content BLOB NULL,"
    "system_profile BLOB NULL,"
    "FOREIGN KEY(uuid) REFERENCES local_traces_index(uuid) ON DELETE CASCADE)";
// clang-format on

}  // namespace

BaseTraceReport::BaseTraceReport() = default;
BaseTraceReport::BaseTraceReport(const BaseTraceReport& other) = default;
BaseTraceReport::~BaseTraceReport() = default;

NewTraceReport::NewTraceReport() = default;
NewTraceReport::~NewTraceReport() = default;

ClientTraceReport::ClientTraceReport() = default;
ClientTraceReport::~ClientTraceReport() = default;

TraceReportDatabase::TraceReportDatabase()
    : database_(sql::DatabaseOptions().set_cache_size(128),
                /*tag=*/"LocalTraces") {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

bool TraceReportDatabase::OpenDatabase(const base::FilePath& path) {
  if (database_.is_open()) {
    DCHECK_EQ(db_file_path_, path.Append(kLocalTracesDatabasePath));
    return EnsureTableCreated();
  }

  db_file_path_ = path.Append(kLocalTracesDatabasePath);

  const base::FilePath dir = db_file_path_.DirName();
  if (!base::DirectoryExists(dir) && !base::CreateDirectory(dir)) {
    return false;
  }

  if (!database_.Open(db_file_path_)) {
    return false;
  }

  return EnsureTableCreated();
}

bool TraceReportDatabase::OpenDatabaseInMemoryForTesting() {
  if (database_.is_open()) {
    return EnsureTableCreated();
  }

  if (!database_.OpenInMemory()) {
    return false;
  }

  return EnsureTableCreated();
}

bool TraceReportDatabase::OpenDatabaseIfExists(const base::FilePath& path) {
  if (database_.is_open()) {
    DCHECK_EQ(db_file_path_, path.Append(kLocalTracesDatabasePath));
    return database_.DoesTableExist(kLocalTracesIndexTableName) &&
           database_.DoesTableExist(kLocalTracesPayloadsTableName);
  }

  db_file_path_ = path.Append(kLocalTracesDatabasePath);
  const base::FilePath dir = db_file_path_.DirName();
  if (!base::DirectoryExists(dir)) {
    return false;
  }

  if (!database_.Open(db_file_path_)) {
    return false;
  }

  if (!database_.DoesTableExist(kLocalTracesIndexTableName) ||
      !database_.DoesTableExist(kLocalTracesPayloadsTableName)) {
    return false;
  }

  return EnsureTableCreated();
}

bool TraceReportDatabase::AddTrace(const NewTraceReport& new_report) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_initialized()) {
    return false;
  }

  static constexpr char kCreateIndexSql[] =
      // clang-format off
      "INSERT INTO local_traces_index("
        "uuid, creation_time, scenario_name, upload_rule_name,"
        "upload_rule_value, state, upload_time, skip_reason,"
        "file_size) VALUES(?,?,?,?,?,?,?,?,?)";
  // clang-format on
  sql::Statement create_index_entry(
      database_.GetCachedStatement(SQL_FROM_HERE, kCreateIndexSql));
  CHECK(create_index_entry.is_valid());

  create_index_entry.BindString(0, new_report.uuid.ToString());
  create_index_entry.BindTime(1, new_report.creation_time);
  create_index_entry.BindString(2, new_report.scenario_name);
  create_index_entry.BindString(3, new_report.upload_rule_name);
  if (new_report.upload_rule_value) {
    create_index_entry.BindInt(4, *new_report.upload_rule_value);
  } else {
    create_index_entry.BindNull(4);
  }
  create_index_entry.BindInt(
      5, new_report.skip_reason == SkipUploadReason::kNoSkip
             ? static_cast<int>(ReportUploadState::kPending)
             : static_cast<int>(ReportUploadState::kNotUploaded));
  create_index_entry.BindNull(6);
  create_index_entry.BindInt(7, static_cast<int>(new_report.skip_reason));
  create_index_entry.BindInt64(8, new_report.total_size);

  static constexpr char kCreatePayloadSql[] =
      // clang-format off
      "INSERT INTO local_traces_payloads("
        "uuid, trace_content, system_profile) VALUES(?,?,?)";
  // clang-format on
  sql::Statement create_payload(
      database_.GetCachedStatement(SQL_FROM_HERE, kCreatePayloadSql));
  CHECK(create_payload.is_valid());

  create_payload.BindString(0, new_report.uuid.ToString());
  create_payload.BindBlob(1, new_report.trace_content);
  create_payload.BindBlob(2, new_report.system_profile);

  sql::Transaction transaction(&database_);
  if (!transaction.Begin()) {
    return false;
  }
  create_index_entry.Run();
  if (!new_report.trace_content.empty()) {
    create_payload.Run();
  }
  return transaction.Commit();
}

bool TraceReportDatabase::UserRequestedUpload(const base::Token& uuid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_initialized()) {
    return false;
  }

  static constexpr char kUpdateIndexSql[] =
      // clang-format off
      "UPDATE local_traces_index "
        "SET state=? "
        "WHERE uuid=? "
        "AND NOT skip_reason=?";
  // clang-format on
  sql::Statement update_local_trace(
      database_.GetCachedStatement(SQL_FROM_HERE, kUpdateIndexSql));
  CHECK(update_local_trace.is_valid());

  update_local_trace.BindInt(
      0, static_cast<int>(ReportUploadState::kPending_UserRequested));
  update_local_trace.BindString(1, uuid.ToString());
  update_local_trace.BindInt(
      2, static_cast<int>(SkipUploadReason::kNotAnonymized));

  return update_local_trace.Run();
}

bool TraceReportDatabase::UploadComplete(const base::Token& uuid,
                                         base::Time time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!is_initialized()) {
    return false;
  }

  static constexpr char kUpdateIndexSql[] =
      // clang-format off
      "UPDATE local_traces_index "
        "SET state=?, upload_time=? "
        "WHERE uuid=?";
  // clang-format on
  sql::Statement update_index(
      database_.GetCachedStatement(SQL_FROM_HERE, kUpdateIndexSql));
  CHECK(update_index.is_valid());

  update_index.BindInt(0, static_cast<int>(ReportUploadState::kUploaded));
  update_index.BindTime(1, time);
  update_index.BindString(2, uuid.ToString());

  return update_index.Run();
}

bool TraceReportDatabase::UploadSkipped(const base::Token& uuid,
                                        SkipUploadReason skip_reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!is_initialized()) {
    return false;
  }

  static constexpr char kUpdateIndexSql[] =
      // clang-format off
      "UPDATE local_traces_index "
        "SET state=?, skip_reason=? "
        "WHERE uuid=?";
  // clang-format on
  sql::Statement update_index(
      database_.GetCachedStatement(SQL_FROM_HERE, kUpdateIndexSql));
  CHECK(update_index.is_valid());

  update_index.BindInt(0, static_cast<int>(ReportUploadState::kNotUploaded));
  update_index.BindInt(1, static_cast<int>(skip_reason));
  update_index.BindString(2, uuid.ToString());

  return update_index.Run();
}

std::optional<std::string> TraceReportDatabase::GetTraceContent(
    const base::Token& uuid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_initialized()) {
    return std::nullopt;
  }

  static constexpr char kGetContentSql[] =
      // clang-format off
      "SELECT trace_content FROM local_traces_payloads "
        "WHERE uuid=?";
  // clang-format on
  sql::Statement get_content(
      database_.GetCachedStatement(SQL_FROM_HERE, kGetContentSql));

  CHECK(get_content.is_valid());

  get_content.BindString(0, uuid.ToString());

  if (!get_content.Step()) {
    return std::nullopt;
  }

  std::string received_value = get_content.ColumnString(0);

  if (received_value.empty()) {
    return std::nullopt;
  }
  return received_value;
}

std::optional<std::string> TraceReportDatabase::GetSystemProfile(
    const base::Token& uuid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_initialized()) {
    return std::nullopt;
  }

  static constexpr char kGetSystemProfileSql[] =
      // clang-format off
      "SELECT system_profile FROM local_traces_payloads "
        "WHERE uuid=?";
  // clang-format on
  sql::Statement get_system_profile(
      database_.GetCachedStatement(SQL_FROM_HERE, kGetSystemProfileSql));

  CHECK(get_system_profile.is_valid());
  get_system_profile.BindString(0, uuid.ToString());

  if (!get_system_profile.Step()) {
    return std::nullopt;
  }

  std::string received_value = get_system_profile.ColumnString(0);

  if (received_value.empty()) {
    return std::nullopt;
  }
  return received_value;
}

bool TraceReportDatabase::DeleteTrace(const base::Token& uuid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_initialized()) {
    return false;
  }

  sql::Statement delete_index_entry(database_.GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM local_traces_index WHERE uuid=?"));
  CHECK(delete_index_entry.is_valid());
  delete_index_entry.BindString(0, uuid.ToString());

  return delete_index_entry.Run();
}

bool TraceReportDatabase::DeleteAllTraces() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_initialized()) {
    return false;
  }

  sql::Statement delete_index_entries(database_.GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM local_traces_index"));
  CHECK(delete_index_entries.is_valid());
  return delete_index_entries.Run();
}

bool TraceReportDatabase::DeleteTracesInDateRange(base::Time start,
                                                  base::Time end) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_initialized()) {
    return false;
  }

  sql::Statement delete_index_entries(database_.GetCachedStatement(
      SQL_FROM_HERE,
      "DELETE FROM local_traces_index WHERE creation_time BETWEEN ? AND ?"));
  CHECK(delete_index_entries.is_valid());
  delete_index_entries.BindTime(0, start);
  delete_index_entries.BindTime(1, end);

  return delete_index_entries.Run();
}

bool TraceReportDatabase::DeleteTraceReportsOlderThan(base::TimeDelta age) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_initialized()) {
    return false;
  }

  static constexpr char kDeleteReportsOlderThanSql[] =
      // clang-format off
      "DELETE FROM local_traces_index "
        "WHERE creation_time < ?";
  // clang-format on
  sql::Statement delete_reports_older_than(
      database_.GetCachedStatement(SQL_FROM_HERE, kDeleteReportsOlderThanSql));

  delete_reports_older_than.BindTime(0, base::Time(base::Time::Now() - age));
  CHECK(delete_reports_older_than.is_valid());

  return delete_reports_older_than.Run();
}

bool TraceReportDatabase::DeleteUploadedTraceContentOlderThan(
    base::TimeDelta age) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_initialized()) {
    return false;
  }

  static constexpr char kDeletePayloadSql[] =
      // clang-format off
      "DELETE FROM local_traces_payloads "
        "WHERE uuid IN ("
          "SELECT uuid FROM local_traces_index "
          "WHERE state=? AND upload_time < ?)";
  // clang-format on
  sql::Statement delete_payload(
      database_.GetCachedStatement(SQL_FROM_HERE, kDeletePayloadSql));
  CHECK(delete_payload.is_valid());

  delete_payload.BindInt(0, static_cast<int>(ReportUploadState::kUploaded));
  delete_payload.BindTime(1, base::Time(base::Time::Now() - age));
  return delete_payload.Run();
}

bool TraceReportDatabase::DeleteOldTraceContent(size_t max_traces) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_initialized()) {
    return false;
  }

  static constexpr char kDeletePayloadSql[] =
      // clang-format off
      "DELETE FROM local_traces_payloads "
        "WHERE uuid NOT IN ("
          "SELECT uuid "
          "FROM local_traces_index "
          "ORDER BY creation_time DESC "
          "LIMIT ?)";
  // clang-format on
  sql::Statement delete_payload(
      database_.GetCachedStatement(SQL_FROM_HERE, kDeletePayloadSql));
  CHECK(delete_payload.is_valid());

  delete_payload.BindInt(0, static_cast<int>(max_traces));

  return delete_payload.Run();
}

bool TraceReportDatabase::AllPendingUploadSkipped(
    SkipUploadReason skip_reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_initialized()) {
    return false;
  }

  static constexpr char kUpdateStateSql[] =
      // clang-format off
      "UPDATE local_traces_index "
        "SET state=?, skip_reason=? "
        "WHERE state=?";
  // clang-format on
  sql::Statement statement(
      database_.GetCachedStatement(SQL_FROM_HERE, kUpdateStateSql));
  CHECK(statement.is_valid());

  statement.BindInt(0, static_cast<int>(ReportUploadState::kNotUploaded));
  statement.BindInt(1, static_cast<int>(skip_reason));
  statement.BindInt(2, static_cast<int>(ReportUploadState::kPending));

  return statement.Run();
}

std::vector<ClientTraceReport> TraceReportDatabase::GetAllReports() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<ClientTraceReport> all_reports;

  if (!is_initialized()) {
    return all_reports;
  }

  static constexpr char kGetAllReportsSql[] =
      // clang-format off
      "SELECT i.uuid, creation_time, scenario_name, upload_rule_name, "
        "upload_rule_value, state, upload_time, skip_reason, "
        "file_size, p.uuid IS NOT NULL AS has_trace_content "
        "FROM local_traces_index AS i "
        "LEFT JOIN local_traces_payloads AS p ON i.uuid = p.uuid "
        "ORDER BY creation_time DESC";
  // clang-format on
  sql::Statement statement(
      database_.GetCachedStatement(SQL_FROM_HERE, kGetAllReportsSql));
  CHECK(statement.is_valid());

  while (statement.Step()) {
    all_reports.push_back(GetReportFromStatement(statement));
  }
  return all_reports;
}

std::optional<ClientTraceReport>
TraceReportDatabase::GetNextReportPendingUpload() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_initialized()) {
    return std::nullopt;
  }

  static constexpr char kGetNextReportSql[] =
      // clang-format off
      "SELECT i.uuid, creation_time, scenario_name, upload_rule_name, "
        "upload_rule_value, state, upload_time, skip_reason, "
        "file_size, p.uuid IS NOT NULL AS has_trace_content "
        "FROM local_traces_index AS i "
        "LEFT JOIN local_traces_payloads AS p ON i.uuid = p.uuid "
        "WHERE state in (1,2) "
        "ORDER BY creation_time DESC";
  // clang-format on
  sql::Statement statement(
      database_.GetCachedStatement(SQL_FROM_HERE, kGetNextReportSql));
  CHECK(statement.is_valid());

  // Select the most recent report first, to prioritize surfacing new
  // issues and collecting traces from new scenarios.
  while (statement.Step()) {
    return GetReportFromStatement(statement);
  }
  return std::nullopt;
}

std::optional<size_t> TraceReportDatabase::UploadCountSince(
    const std::string& scenario_name,
    const std::string& upload_rule_name,
    base::Time since) {
  if (!is_initialized()) {
    return std::nullopt;
  }

  static constexpr char kGetUploadCountSql[] =
      // clang-format off
      "SELECT COUNT(uuid) FROM local_traces_index "
        "WHERE scenario_name = ? "
        "AND upload_rule_name = ? "
        "AND creation_time > ? "
        "AND skip_reason=?";
  // clang-format on
  sql::Statement statement(
      database_.GetCachedStatement(SQL_FROM_HERE, kGetUploadCountSql));
  statement.BindString(0, scenario_name);
  statement.BindString(1, upload_rule_name);
  statement.BindTime(2, since);
  statement.BindInt(3, static_cast<int>(SkipUploadReason::kNoSkip));
  CHECK(statement.is_valid());

  while (statement.Step()) {
    return static_cast<uint64_t>(statement.ColumnInt64(0));
  }
  return std::nullopt;
}

base::flat_map<std::string, size_t> TraceReportDatabase::GetScenarioCountsSince(
    base::Time since) {
  base::flat_map<std::string, size_t> scenario_counts;
  if (!is_initialized()) {
    return scenario_counts;
  }

  static constexpr char kGetScenarioCountSql[] =
      // clang-format off
      "SELECT scenario_name, COUNT(uuid) "
        "FROM local_traces_index "
        "WHERE creation_time > ? "
        "GROUP BY scenario_name";
  // clang-format on
  sql::Statement statement(
      database_.GetCachedStatement(SQL_FROM_HERE, kGetScenarioCountSql));
  statement.BindTime(0, since);
  CHECK(statement.is_valid());

  while (statement.Step()) {
    scenario_counts.emplace(statement.ColumnString(0),
                            static_cast<uint64_t>(statement.ColumnInt64(1)));
  }
  return scenario_counts;
}

bool TraceReportDatabase::EnsureTableCreated() {
  DCHECK(database_.is_open());

  if (initialized_) {
    return true;
  }

  sql::Transaction transaction(&database_);
  if (!transaction.Begin()) {
    return false;
  }

  sql::MetaTable meta_table;
  bool has_metatable = meta_table.DoesTableExist(&database_);
  bool has_index_table = database_.DoesTableExist(kLocalTracesIndexTableName);
  if (!has_metatable && has_index_table) {
    // Existing DB with no meta table. Cannot determine DB version.
    if (!database_.Raze()) {
      return false;
    }
  }

  if (!meta_table.Init(&database_, kCurrentVersionNumber,
                       kCurrentVersionNumber)) {
    return false;
  }
  if (meta_table.GetVersionNumber() > kCurrentVersionNumber) {
    return false;
  }
  if (meta_table.GetVersionNumber() < kCurrentVersionNumber) {
    if (!database_.Execute("DROP TABLE IF EXISTS local_traces")) {
      return false;
    }
    if (!database_.Execute("DROP TABLE IF EXISTS local_traces_index")) {
      return false;
    }
    if (!database_.Execute("DROP TABLE IF EXISTS local_traces_payloads")) {
      return false;
    }
    if (!meta_table.SetVersionNumber(kCurrentVersionNumber)) {
      return false;
    }
  }
  initialized_ = database_.Execute(kLocalTracesIndexTableSql) &&
                 database_.Execute(kLocalTracesPayloadsTableSql);

  return initialized_ && transaction.Commit();
}

}  // namespace content
