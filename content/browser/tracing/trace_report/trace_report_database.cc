// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/trace_report/trace_report_database.h"

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
const char kLocalTracesTableName[] = "local_traces";
constexpr int kCurrentVersionNumber = 4;

ClientTraceReport GetReportFromStatement(sql::Statement& statement) {
  auto trace_id = base::Token::FromString(statement.ColumnString(0));
  CHECK(trace_id.has_value());

  ClientTraceReport client_report;
  client_report.uuid = *trace_id;
  client_report.creation_time = statement.ColumnTime(1);
  client_report.scenario_name = statement.ColumnString(2);
  client_report.upload_rule_name = statement.ColumnString(3);

  client_report.upload_state =
      static_cast<ReportUploadState>(statement.ColumnInt(4));
  client_report.upload_time = statement.ColumnTime(5);
  client_report.skip_reason =
      static_cast<SkipUploadReason>(statement.ColumnInt(6));
  client_report.has_trace_content = statement.ColumnBool(7);
  client_report.total_size = static_cast<uint64_t>(statement.ColumnInt64(8));

  return client_report;
}

// create table `local_traces` with following columns:
// `uuid` is the unique ID of the trace.
// `creation_time` The date and time in seconds when the row was created.
// `scenario_name` The trace scenario name.
// `upload_rule_name` The name of the rule that triggered the upload.
// `state` The current upload state of the trace.
// `upload_time` Time at which the trace was uploaded. NULL if not uploaded.
// `skip_reason` Reason why a trace was not uploaded.
// `trace_content` The serialized trace content string
// `system_profile` The serialized system profile string
// `file_size` The size of trace in bytes.
constexpr char kLocalTracesTableSql[] = R"sql(
  CREATE TABLE IF NOT EXISTS local_traces(
    uuid TEXT PRIMARY KEY NOT NULL,
    creation_time DATETIME NOT NULL,
    scenario_name TEXT NOT NULL,
    upload_rule_name TEXT NOT NULL,
    state INT NOT NULL,
    upload_time DATETIME NULL,
    skip_reason INT NOT NULL,
    trace_content BLOB NULL,
    system_profile BLOB NULL,
    file_size INTEGER NOT NULL)
)sql";

}  // namespace

BaseTraceReport::BaseTraceReport() = default;
BaseTraceReport::BaseTraceReport(const BaseTraceReport& other) = default;
BaseTraceReport::~BaseTraceReport() = default;

NewTraceReport::NewTraceReport() = default;
NewTraceReport::~NewTraceReport() = default;

ClientTraceReport::ClientTraceReport() = default;
ClientTraceReport::~ClientTraceReport() = default;

TraceReportDatabase::TraceReportDatabase()
    : database_(sql::DatabaseOptions{.page_size = 4096, .cache_size = 128}) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

bool TraceReportDatabase::OpenDatabase(const base::FilePath& path) {
  if (database_.is_open()) {
    DCHECK_EQ(db_file_path_, path.Append(kLocalTracesDatabasePath));
    return EnsureTableCreated();
  }

  db_file_path_ = path.Append(kLocalTracesDatabasePath);

  // For logging memory dumps
  database_.set_histogram_tag("LocalTraces");

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
    return database_.DoesTableExist(kLocalTracesTableName);
  }

  db_file_path_ = path.Append(kLocalTracesDatabasePath);
  const base::FilePath dir = db_file_path_.DirName();
  if (!base::DirectoryExists(dir)) {
    return false;
  }

  if (!database_.Open(db_file_path_)) {
    return false;
  }

  if (!database_.DoesTableExist(kLocalTracesTableName)) {
    return false;
  }

  return EnsureTableCreated();
}

bool TraceReportDatabase::AddTrace(const NewTraceReport& new_report) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_initialized()) {
    return false;
  }

  sql::Statement create_local_trace(database_.GetCachedStatement(
      SQL_FROM_HERE, R"sql(INSERT INTO local_traces(
                                   uuid, creation_time, scenario_name,
                                   upload_rule_name, state, upload_time,
                                   skip_reason, trace_content, file_size,
                                   system_profile)
                                   VALUES(?,?,?,?,?,?,?,?,?,?)
                                  )sql"));

  CHECK(create_local_trace.is_valid());

  create_local_trace.BindString(0, new_report.uuid.ToString());
  create_local_trace.BindTime(1, new_report.creation_time);
  create_local_trace.BindString(2, new_report.scenario_name);
  create_local_trace.BindString(3, new_report.upload_rule_name);
  create_local_trace.BindInt(
      4, new_report.skip_reason == SkipUploadReason::kNoSkip
             ? static_cast<int>(ReportUploadState::kPending)
             : static_cast<int>(ReportUploadState::kNotUploaded));
  create_local_trace.BindNull(5);
  create_local_trace.BindInt(6, static_cast<int>(new_report.skip_reason));
  if (!new_report.trace_content.empty()) {
    create_local_trace.BindBlob(7, new_report.trace_content);
  } else {
    create_local_trace.BindNull(7);
  }
  create_local_trace.BindInt64(8, new_report.total_size);
  create_local_trace.BindBlob(9, new_report.system_profile);

  return create_local_trace.Run();
}

bool TraceReportDatabase::UserRequestedUpload(const base::Token& uuid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_initialized()) {
    return false;
  }

  sql::Statement update_local_trace(
      database_.GetCachedStatement(SQL_FROM_HERE,
                                   "UPDATE local_traces "
                                   "SET state=? "
                                   "WHERE uuid=?"
                                   "AND NOT skip_reason=?"));

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

  sql::Statement update_local_trace(
      database_.GetCachedStatement(SQL_FROM_HERE,
                                   R"sql(UPDATE local_traces
                                   SET state=?, upload_time=?,
                                   trace_content=NULL,
                                   system_profile=NULL
                                   WHERE uuid=?)sql"));

  CHECK(update_local_trace.is_valid());

  update_local_trace.BindInt(0, static_cast<int>(ReportUploadState::kUploaded));
  update_local_trace.BindTime(1, time);
  update_local_trace.BindString(2, uuid.ToString());

  return update_local_trace.Run();
}

bool TraceReportDatabase::UploadSkipped(const base::Token& uuid,
                                        SkipUploadReason skip_reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!is_initialized()) {
    return false;
  }

  sql::Statement update_local_trace(
      database_.GetCachedStatement(SQL_FROM_HERE,
                                   R"sql(UPDATE local_traces
                                   SET state=?, skip_reason=?
                                   WHERE uuid=?)sql"));

  CHECK(update_local_trace.is_valid());

  update_local_trace.BindInt(0,
                             static_cast<int>(ReportUploadState::kNotUploaded));
  update_local_trace.BindInt(1, static_cast<int>(skip_reason));
  update_local_trace.BindString(2, uuid.ToString());

  return update_local_trace.Run();
}

std::optional<std::string> TraceReportDatabase::GetTraceContent(
    const base::Token& uuid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_initialized()) {
    return std::nullopt;
  }

  sql::Statement get_local_trace_content(database_.GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT trace_content FROM local_traces WHERE "
      "uuid=?"));

  CHECK(get_local_trace_content.is_valid());

  get_local_trace_content.BindString(0, uuid.ToString());

  if (!get_local_trace_content.Step()) {
    return std::nullopt;
  }

  std::string received_value = get_local_trace_content.ColumnString(0);

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

  sql::Statement get_system_profile(database_.GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT system_profile FROM local_traces WHERE "
      "uuid=?"));

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

  sql::Statement delete_trace(database_.GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM local_traces WHERE uuid=?"));

  CHECK(delete_trace.is_valid());

  delete_trace.BindString(0, uuid.ToString());

  return delete_trace.Run();
}

bool TraceReportDatabase::DeleteAllTraces() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_initialized()) {
    return false;
  }

  sql::Statement delete_all_traces(
      database_.GetCachedStatement(SQL_FROM_HERE, "DELETE FROM local_traces"));

  CHECK(delete_all_traces.is_valid());

  return delete_all_traces.Run();
}

bool TraceReportDatabase::DeleteTracesInDateRange(const base::Time start,
                                                  const base::Time end) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_initialized()) {
    return false;
  }

  sql::Statement delete_traces_in_range(database_.GetCachedStatement(
      SQL_FROM_HERE,
      "DELETE FROM local_traces WHERE creation_time BETWEEN ? AND ?"));

  delete_traces_in_range.BindTime(0, start);
  delete_traces_in_range.BindTime(1, end);

  CHECK(delete_traces_in_range.is_valid());

  return delete_traces_in_range.Run();
}

bool TraceReportDatabase::DeleteTraceReportsOlderThan(base::TimeDelta age) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_initialized()) {
    return false;
  }

  sql::Statement delete_reports_older_than(
      database_.GetCachedStatement(SQL_FROM_HERE, R"sql(
        DELETE FROM local_traces
        WHERE creation_time < ?)sql"));

  delete_reports_older_than.BindTime(0, base::Time(base::Time::Now() - age));

  CHECK(delete_reports_older_than.is_valid());

  return delete_reports_older_than.Run();
}

bool TraceReportDatabase::DeleteOldTraceContent(size_t max_traces) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_initialized()) {
    return false;
  }

  sql::Statement delete_old_trace_content(
      database_.GetCachedStatement(SQL_FROM_HERE, R"sql(
        UPDATE local_traces
        SET trace_content = null
        WHERE state=? and uuid not in (
          SELECT uuid
          FROM local_traces
          WHERE trace_content IS NOT NULL
          ORDER BY creation_time DESC
          LIMIT ?)
        )sql"));

  delete_old_trace_content.BindInt(
      0, static_cast<int>(ReportUploadState::kNotUploaded));
  delete_old_trace_content.BindInt(1, static_cast<int>(max_traces));

  CHECK(delete_old_trace_content.is_valid());

  return delete_old_trace_content.Run();
}

bool TraceReportDatabase::AllPendingUploadSkipped(
    SkipUploadReason skip_reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_initialized()) {
    return false;
  }

  sql::Statement statement(
      database_.GetCachedStatement(SQL_FROM_HERE,
                                   R"sql(UPDATE local_traces
                                   SET state=?, skip_reason=?
                                   WHERE state=?)sql"));

  statement.BindInt(0, static_cast<int>(ReportUploadState::kNotUploaded));
  statement.BindInt(1, static_cast<int>(skip_reason));
  statement.BindInt(2, static_cast<int>(ReportUploadState::kPending));

  CHECK(statement.is_valid());

  return statement.Run();
}

bool TraceReportDatabase::EnsureTableCreated() {
  DCHECK(database_.is_open());

  if (initialized_) {
    return true;
  }

  sql::MetaTable meta_table;
  bool has_metatable = meta_table.DoesTableExist(&database_);
  bool has_schema = database_.DoesTableExist(kLocalTracesTableName);
  if (!has_metatable && has_schema) {
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
    if (!database_.Execute("DROP TABLE local_traces")) {
      return false;
    }
    if (!meta_table.SetVersionNumber(kCurrentVersionNumber)) {
      return false;
    }
  }
  initialized_ = database_.Execute(kLocalTracesTableSql);

  return initialized_;
}

std::vector<ClientTraceReport> TraceReportDatabase::GetAllReports() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<ClientTraceReport> all_reports;

  if (!is_initialized()) {
    return all_reports;
  }

  sql::Statement statement(database_.GetCachedStatement(SQL_FROM_HERE, R"sql(
      SELECT uuid, creation_time, scenario_name, upload_rule_name,
        state, upload_time, skip_reason,
        trace_content IS NOT NULL as has_trace_content, file_size
      FROM local_traces
      ORDER BY creation_time DESC
    )sql"));
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

  sql::Statement statement(database_.GetCachedStatement(SQL_FROM_HERE, R"sql(
      SELECT uuid, creation_time, scenario_name, upload_rule_name,
        state, upload_time, skip_reason,
        trace_content IS NOT NULL as has_trace_content, file_size
      FROM local_traces WHERE state in (1,2)
      ORDER BY creation_time DESC
    )sql"));
  CHECK(statement.is_valid());

  // Select the most recent report first, to prioritize surfacing new
  // issues and collecting traces from new scenarios.
  while (statement.Step()) {
    return GetReportFromStatement(statement);
  }
  return std::nullopt;
}

std::optional<size_t> TraceReportDatabase::UploadCountSince(
    std::string scenario_name,
    base::Time since) {
  if (!is_initialized()) {
    return std::nullopt;
  }

  sql::Statement statement(database_.GetCachedStatement(SQL_FROM_HERE, R"sql(
      SELECT COUNT(uuid) FROM local_traces
      WHERE scenario_name = ? AND creation_time > ?
      AND skip_reason=?
    )sql"));
  statement.BindString(0, scenario_name);
  statement.BindTime(1, since);
  statement.BindInt(2, static_cast<int>(SkipUploadReason::kNoSkip));
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

  sql::Statement statement(
      database_.GetCachedStatement(SQL_FROM_HERE,
                                   R"sql(SELECT scenario_name, COUNT(uuid) FROM
                                   local_traces
                                   WHERE creation_time > ?
                                   GROUP BY scenario_name)sql"));
  statement.BindTime(0, since);
  CHECK(statement.is_valid());

  while (statement.Step()) {
    scenario_counts.emplace(statement.ColumnString(0),
                            static_cast<uint64_t>(statement.ColumnInt64(1)));
  }
  return scenario_counts;
}

}  // namespace content
