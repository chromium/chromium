// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_TRACE_REPORT_DATABASE_H_
#define CONTENT_BROWSER_TRACING_TRACE_REPORT_DATABASE_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "content/common/content_export.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

class CONTENT_EXPORT TraceReportDatabase {
 public:
  enum class ReportUploadState {
    kNotUploaded = 0,
    kPending,
    kPending_UserRequested,
    kUploaded
  };

  enum class SkipUploadReason { kNoSkip = 0 };

  // BaseReport represents the base metadata that use fundamental to create or
  // display a report.
  struct CONTENT_EXPORT BaseReport {
    BaseReport();
    BaseReport(const BaseReport& base_report);
    ~BaseReport();
    // A unique identifier by which this report will always be known to the
    // database as well as outside of it (e.g.: perfetto).
    base::Uuid uuid;

    // The time at which the report was created.
    base::Time creation_time;

    // The name of the scenario that triggered this trace to be collected and
    // report to be created.
    std::string scenario_name;

    // The upload rule name this report needs to respect for this report to be
    // uploaded.
    std::string upload_rule_name;

    // The total size in bytes taken by the report.
    uint64_t total_size;

    // The reason for which a report was not uploaded even if the upload rules
    // were met.
    SkipUploadReason skip_reason = SkipUploadReason::kNoSkip;
  };

  // NewReport represents the metadata needed to create and add a new report
  // into the TraceReportDatabase.
  struct CONTENT_EXPORT NewReport : public BaseReport {
    NewReport();
    ~NewReport();

    NewReport(const NewReport& new_report) = delete;
    NewReport(NewReport&& new_report) = default;

    NewReport& operator=(const NewReport& new_report) = delete;
    NewReport& operator=(NewReport&& new_report) = default;

    // The string containing the trace for this report.
    std::string proto;
  };

  // ClientReport represents all metadata to be displayed to user. Proto member
  // is not included here since it can be of significant size. Therefore, if
  // proto is needed it can be obtained through |GetProtoValue|.
  struct CONTENT_EXPORT ClientReport : public BaseReport {
    ClientReport();
    ~ClientReport();
    // The current upload state for this report represented by
    // ReportUploadState.
    ReportUploadState state;

    // The time at which the report was successfully uploaded to a server.
    base::Time upload_time;
  };

  TraceReportDatabase();
  ~TraceReportDatabase() = default;

  TraceReportDatabase(const TraceReportDatabase&) = delete;
  TraceReportDatabase& operator=(const TraceReportDatabase&) = delete;

  bool OpenDatabase(const base::FilePath& path);

  // Open database only if it already exists.
  bool OpenDatabaseIfExists(const base::FilePath& path);

  // Initialize DB and open in it memory.
  bool OpenDatabaseForTesting();

  // Adds a new row (trace) to the local_traces table.
  bool AddTrace(NewReport new_report);

  // Delete a row (trace) from the local_traces table.
  bool DeleteTrace(base::Uuid uuid);

  // Deletes all rows (traces) from the local_traces.
  bool DeleteAllTraces();

  // Delete traces between the |start| and |end| dates inclusively.
  bool DeleteTracesInDateRange(const base::Time start, const base::Time end);

  // Delete all traces older than |age| from today.
  bool DeleteTracesOlderThan(const base::TimeDelta age);

  bool UserRequestedUpload(base::Uuid uuid);
  bool UploadComplete(base::Uuid uuid, base::Time time);
  bool UploadSkipped(base::Uuid uuid);

  // Get string if the current Trace exists.
  absl::optional<std::string> GetProtoValue(base::Uuid uuid);

  // Returns all the reports currently stored in the database.
  std::vector<ClientReport> GetAllReports();

  // Returns the next report pending upload.
  absl::optional<ClientReport> GetNextReportPendingUpload();

 private:
  bool EnsureTableCreated();

  sql::Database database_;
  base::FilePath db_file_path_;

  bool initialized_ = false;

  // Guards usage of |database_|
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRACING_TRACE_REPORT_DATABASE_H_
