// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_TRACE_REPORT_TRACE_REPORT_DATABASE_H_
#define CONTENT_BROWSER_TRACING_TRACE_REPORT_TRACE_REPORT_DATABASE_H_

#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/token.h"
#include "content/common/content_export.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace content {

enum class ReportUploadState {
  kNotUploaded = 0,
  kPending,
  kPending_UserRequested,
  kUploaded
};

enum class SkipUploadReason {
  kNoSkip = 0,
  kSizeLimitExceeded = 1,
  kNotAnonymized = 2,
  kScenarioQuotaExceeded = 3,
  kUploadTimedOut = 4,
};

// BaseTraceReport contains common data used to create and display a trace
// report.
struct CONTENT_EXPORT BaseTraceReport {
  BaseTraceReport();
  BaseTraceReport(const BaseTraceReport& other);
  ~BaseTraceReport();
  // A unique identifier by which this report will always be known to the
  // database as well as outside of it (e.g.: perfetto).
  base::Token uuid;

  // The time at which the report was created.
  base::Time creation_time;

  // The name of the scenario that triggered this trace to be collected and
  // report to be created.
  std::string scenario_name;

  // The upload rule name this report needs to respect for this report to be
  // uploaded.
  std::string upload_rule_name;

  // The total size in bytes taken by the report.
  uint64_t total_size = 0;

  // The reason for which a report was not uploaded even if the upload rules
  // were met.
  SkipUploadReason skip_reason = SkipUploadReason::kNoSkip;
};

// NewTraceReport represents the metadata needed to create and add a new
// report into the TraceReportDatabase.
struct CONTENT_EXPORT NewTraceReport : BaseTraceReport {
  NewTraceReport();
  // NOLINTNEXTLINE(google-explicit-constructor)
  NewTraceReport(const BaseTraceReport& report) : BaseTraceReport(report) {}
  ~NewTraceReport();

  NewTraceReport(const NewTraceReport& new_report) = delete;
  NewTraceReport(NewTraceReport&& new_report) = default;

  NewTraceReport& operator=(const NewTraceReport& new_report) = delete;
  NewTraceReport& operator=(NewTraceReport&& new_report) = default;

  // The serialized trace report content.
  std::string trace_content;

  // The serialized system profile information.
  std::string system_profile;
};

// ClientTraceReport represents all metadata of a trace report to be displayed
// to user. Proto member is not included here since it can be of significant
// size. Therefore, if proto is needed it can be obtained through
// GetProtoValue().
struct CONTENT_EXPORT ClientTraceReport : BaseTraceReport {
  ClientTraceReport();
  ~ClientTraceReport();

  // The current upload state for this report represented by
  // ReportUploadState.
  ReportUploadState upload_state = ReportUploadState::kNotUploaded;

  // The time at which the report was successfully uploaded to a server.
  base::Time upload_time;

  // Whether the report has content (payload) attached to it.
  bool has_trace_content = false;
};

class CONTENT_EXPORT TraceReportDatabase {
 public:
  TraceReportDatabase();
  ~TraceReportDatabase() = default;

  TraceReportDatabase(const TraceReportDatabase&) = delete;
  TraceReportDatabase& operator=(const TraceReportDatabase&) = delete;

  bool is_open() const { return database_.is_open(); }
  bool is_initialized() const { return initialized_; }

  bool OpenDatabase(const base::FilePath& path);

  // Open database only if it already exists.
  bool OpenDatabaseIfExists(const base::FilePath& path);

  // Initialize DB and open in it memory.
  bool OpenDatabaseInMemoryForTesting();

  // Adds a new row (trace) to the local_traces table.
  bool AddTrace(const NewTraceReport& new_report);

  // Delete a row (trace) from the local_traces table.
  bool DeleteTrace(const base::Token& uuid);

  // Deletes all rows (traces) from the local_traces.
  bool DeleteAllTraces();

  // Delete traces between the |start| and |end| dates inclusively.
  bool DeleteTracesInDateRange(const base::Time start, const base::Time end);

  // Delete all reports older than |age| from today.
  bool DeleteTraceReportsOlderThan(const base::TimeDelta age);
  // Delete old trace content, keeping up to `max_traces`.
  bool DeleteOldTraceContent(size_t max_traces);

  // Mark all reports that are pending upload as skipped with `skip_reason`.
  bool AllPendingUploadSkipped(SkipUploadReason skip_reason);

  bool UserRequestedUpload(const base::Token& uuid);
  bool UploadComplete(const base::Token& uuid, base::Time time);
  bool UploadSkipped(const base::Token& uuid, SkipUploadReason skip_reason);

  // Returns the serialized trace content string if any.
  std::optional<std::string> GetTraceContent(const base::Token& uuid);

  // Returns the serialize system profile information if any.
  std::optional<std::string> GetSystemProfile(const base::Token& uuid);

  // Returns the number of trace for |scenario_name| since |since|.
  std::optional<size_t> UploadCountSince(std::string scenario_name,
                                         base::Time since);

  // Returns the saved count per scenario since `since`.
  base::flat_map<std::string, size_t> GetScenarioCountsSince(base::Time since);

  // Returns all the reports currently stored in the database.
  std::vector<ClientTraceReport> GetAllReports();

  // Returns the next report pending upload.
  std::optional<ClientTraceReport> GetNextReportPendingUpload();

 private:
  bool EnsureTableCreated();

  sql::Database database_;
  base::FilePath db_file_path_;

  bool initialized_ = false;

  // Guards usage of |database_|
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRACING_TRACE_REPORT_TRACE_REPORT_DATABASE_H_
