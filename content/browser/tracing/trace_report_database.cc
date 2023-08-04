// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/trace_report_database.h"

#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/uuid.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

namespace {

const base::FilePath::CharType kLocalTracesDatabaseName[] =
    FILE_PATH_LITERAL("LocalTraces.db");
constexpr int kCurrentVersionNumber = 1;

// create table `local_traces` with following columns:
// `uuid` is the unique ID of the trace.
// `creation_time` The date and time in seconds when the row was created.
// `scenario_name` The trace scenario name.
// `upload_rule_name` The name of the rule that triggered the upload.
// `state` The current upload state of the trace.
// `upload_time` Time at which the trace was uploaded. NULL if not uploaded.
// `skip_reason` Reason why a trace was not uploaded.
// `proto` The trace proto string
// `file_size` The size of trace in bytes.
static constexpr char kLocalTracesTableSql[] = R"sql(
  CREATE TABLE IF NOT EXISTS local_traces(
    uuid TEXT PRIMARY KEY NOT NULL,
    creation_time DATETIME NOT NULL,
    scenario_name TEXT NOT NULL,
    upload_rule_name TEXT NOT NULL,
    state INT NOT NULL,
    upload_time DATETIME NULL,
    skip_reason INT NOT NULL,
    proto BLOB NOT NULL,
    file_size INTEGER NOT NULL)
)sql";

}  // namespace

TraceReportDatabase::TraceReportDatabase()
    : database_(sql::DatabaseOptions{.exclusive_locking = true,
                                     .page_size = 4096,
                                     .cache_size = 128}) {}

TraceReportDatabase::BaseReport::BaseReport() = default;
TraceReportDatabase::BaseReport::BaseReport(const BaseReport& other) = default;
TraceReportDatabase::BaseReport::~BaseReport() = default;

TraceReportDatabase::NewReport::NewReport() = default;
TraceReportDatabase::NewReport::~NewReport() = default;

TraceReportDatabase::ClientReport::ClientReport() = default;
TraceReportDatabase::ClientReport::~ClientReport() = default;

bool TraceReportDatabase::OpenDatabase(const base::FilePath& path) {
  if (database_.is_open()) {
    return true;
  }

  db_file_path_ = path.Append(kLocalTracesDatabaseName);

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

bool TraceReportDatabase::OpenDatabaseForTesting() {
  if (database_.is_open()) {
    return true;
  }

  if (!database_.OpenInMemory()) {
    return false;
  }

  return EnsureTableCreated();
}

bool TraceReportDatabase::AddTrace(NewReport new_report) {
  if (!database_.is_open()) {
    return false;
  }

  sql::Statement create_local_trace(database_.GetCachedStatement(
      SQL_FROM_HERE, R"sql(INSERT INTO local_traces(
                                   uuid, creation_time, scenario_name,
                                   upload_rule_name, state, upload_time,
                                   skip_reason, proto, file_size)
                                   VALUES(?,?,?,?,?,?,?,?,?)
                                  )sql"));

  CHECK(create_local_trace.is_valid());

  create_local_trace.BindString(0, new_report.uuid.AsLowercaseString());
  create_local_trace.BindTime(1, new_report.creation_time);
  create_local_trace.BindString(2, new_report.scenario_name);
  create_local_trace.BindString(3, new_report.upload_rule_name);
  create_local_trace.BindInt(4,
                             static_cast<int>(ReportUploadState::kNotUploaded));
  create_local_trace.BindNull(5);
  create_local_trace.BindInt(6, static_cast<int>(SkipUploadReason::kNoSkip));
  create_local_trace.BindBlob(7, new_report.proto);
  create_local_trace.BindInt64(8, new_report.total_size);

  return create_local_trace.Run();
}

bool TraceReportDatabase::UserRequestedUpload(base::Uuid uuid) {
  if (!database_.is_open()) {
    return false;
  }

  sql::Statement update_local_trace(
      database_.GetCachedStatement(SQL_FROM_HERE,
                                   "UPDATE local_traces "
                                   "SET state=? "
                                   "WHERE uuid=?"));

  CHECK(update_local_trace.is_valid());

  update_local_trace.BindInt(
      0, static_cast<int>(ReportUploadState::kPending_UserRequested));
  update_local_trace.BindString(1, uuid.AsLowercaseString());

  return update_local_trace.Run();
}

bool TraceReportDatabase::UploadComplete(base::Uuid uuid, base::Time time) {
  if (!database_.is_open()) {
    return false;
  }

  sql::Statement update_local_trace(
      database_.GetCachedStatement(SQL_FROM_HERE,
                                   "UPDATE local_traces "
                                   "SET state=?, upload_time=? "
                                   "WHERE uuid=?"));

  CHECK(update_local_trace.is_valid());

  update_local_trace.BindInt(0, static_cast<int>(ReportUploadState::kUploaded));
  update_local_trace.BindTime(1, time);
  update_local_trace.BindString(2, uuid.AsLowercaseString());

  return update_local_trace.Run();
}

absl::optional<std::string> TraceReportDatabase::GetProtoValue(
    base::Uuid uuid) {
  if (!database_.is_open()) {
    return absl::nullopt;
  }

  sql::Statement get_local_trace_proto(
      database_.GetCachedStatement(SQL_FROM_HERE,
                                   "SELECT proto FROM local_traces WHERE "
                                   "uuid=?"));

  CHECK(get_local_trace_proto.is_valid());

  get_local_trace_proto.BindString(0, uuid.AsLowercaseString());

  if (!get_local_trace_proto.Step()) {
    return absl::nullopt;
  }

  std::string received_value = get_local_trace_proto.ColumnString(0);

  if (received_value.empty()) {
    return absl::nullopt;
  }
  return received_value;
}

bool TraceReportDatabase::DeleteTrace(base::Uuid uuid) {
  if (!database_.is_open()) {
    return false;
  }

  sql::Statement delete_trace(database_.GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM local_traces WHERE uuid=?"));

  CHECK(delete_trace.is_valid());

  delete_trace.BindString(0, uuid.AsLowercaseString());

  return delete_trace.Run();
}

bool TraceReportDatabase::DeleteAllTraces() {
  if (!database_.is_open()) {
    return false;
  }

  sql::Statement delete_all_traces(
      database_.GetCachedStatement(SQL_FROM_HERE, "DELETE FROM local_traces"));

  CHECK(delete_all_traces.is_valid());

  return delete_all_traces.Run();
}

bool TraceReportDatabase::DeleteTracesInDateRange(const base::Time start,
                                                  const base::Time end) {
  if (!database_.is_open()) {
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

bool TraceReportDatabase::EnsureTableCreated() {
  DCHECK(database_.is_open());

  sql::MetaTable meta_table;
  if (!meta_table.Init(&database_, kCurrentVersionNumber,
                       kCurrentVersionNumber)) {
    return false;
  }

  return database_.Execute(kLocalTracesTableSql);
}

std::vector<TraceReportDatabase::ClientReport>
TraceReportDatabase::GetAllReports() {
  std::vector<TraceReportDatabase::ClientReport> all_reports;

  if (!database_.is_open()) {
    return all_reports;
  }

  sql::Statement get_all_local_trace(database_.GetCachedStatement(
      SQL_FROM_HERE, "SELECT * FROM local_traces"));

  CHECK(get_all_local_trace.is_valid());

  while (get_all_local_trace.Step()) {
    TraceReportDatabase::ClientReport client_report;
    // Initialization of members
    client_report.uuid =
        base::Uuid::ParseLowercase(get_all_local_trace.ColumnString(0));
    client_report.creation_time = get_all_local_trace.ColumnTime(1);
    client_report.scenario_name = get_all_local_trace.ColumnString(2);
    client_report.upload_rule_name = get_all_local_trace.ColumnString(3);
    client_report.total_size =
        static_cast<uint64_t>(get_all_local_trace.ColumnInt64(8));

    client_report.state =
        static_cast<ReportUploadState>(get_all_local_trace.ColumnInt(4));
    client_report.upload_time = get_all_local_trace.ColumnTime(5);
    client_report.skip_reason =
        static_cast<SkipUploadReason>(get_all_local_trace.ColumnInt(6));

    all_reports.push_back(client_report);
  }
  return all_reports;
}

}  // namespace content
