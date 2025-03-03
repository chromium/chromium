// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/crash/crashpad_linux.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/time/time.h"
#include "remoting/base/file_path_util_linux.h"
#include "remoting/base/logging.h"
#include "remoting/base/version.h"
#include "third_party/crashpad/crashpad/client/crash_report_database.h"
#include "third_party/crashpad/crashpad/client/crashpad_client.h"
#include "third_party/crashpad/crashpad/client/settings.h"

namespace remoting {

using crashpad::CrashReportDatabase;

constexpr char kChromotingCrashpadHandler[] = "crashpad-handler";
constexpr char kDefaultCrashpadUploadUrl[] =
    "https://clients2.google.com/cr/report";
const base::FilePath::CharType kChromotingCrashpadDatabasePath[] =
    FILE_PATH_LITERAL("crashpad");

// Maximum number of crash reports to log. Reports are sorted by timestamp so
// the most recent N reports will be logged.
const size_t kMaxReportsToLog = 2;

// Maximum number of crash reports to retain in the database. When the database
// contains more than this number, the oldest ones will be deleted, regardless
// of |kMaxReportDays|.
const size_t kMaxReportsToRetain = 20;

// Maximum number of days to keep reports around in the local database.
const size_t kMaxReportAgeDays = 7;

CrashpadLinux::CrashpadLinux() = default;

bool CrashpadLinux::GetCrashpadHandlerPath(base::FilePath* handler_path) {
  if (!base::PathService::Get(base::DIR_EXE, handler_path)) {
    LOG(ERROR) << "Unable to get exe dir for crashpad handler";
    return false;
  }
  *handler_path = handler_path->Append(kChromotingCrashpadHandler);
  return true;
}

base::FilePath CrashpadLinux::GetCrashpadDatabasePath() {
  base::FilePath database_path = GetConfigDirectoryPath();
  return database_path.Append(kChromotingCrashpadDatabasePath);
}

void CrashpadLinux::LogCrashReportInfo(
    const CrashReportDatabase::Report& report) {
  std::string id = report.id;
  // |id| will only be assigned if the report has been successfully uploaded.
  if (id.empty()) {
    HOST_LOG << "  Crash id: <unassigned>";
  } else {
    HOST_LOG << "  Crash id: " << id << " (http://go/crash/" << id << ")";
  }
  HOST_LOG << "    path: " << report.file_path;
  HOST_LOG << "    uuid: " << report.uuid.ToString();
  HOST_LOG << "    created: " << base::Time::FromTimeT(report.creation_time);
  HOST_LOG << "    uploaded: " << (report.uploaded ? "yes" : "no")
           << " (attempts: " << report.upload_attempts << ")";
}

void CrashpadLinux::SortAndLogCrashReports(
    std::vector<CrashReportDatabase::Report>& reports,
    std::string report_type) {
  size_t num_reports = reports.size();
  if (num_reports > kMaxReportsToLog) {
    HOST_LOG << "Recent " << report_type << " crash reports: " << num_reports
             << " (most recent " << kMaxReportsToLog << " shown)";
  } else {
    HOST_LOG << "Recent " << report_type << " crash reports: " << num_reports;
  }

  // Sort so that the most recent reports are first.
  std::sort(reports.begin(), reports.end(),
            [](CrashReportDatabase::Report const& a,
               CrashReportDatabase::Report const& b) {
              return a.creation_time > b.creation_time;
            });
  for (size_t i = 0; i < num_reports && i < kMaxReportsToLog; ++i) {
    const auto& report = reports[i];
    LogCrashReportInfo(report);
  }
}

void CrashpadLinux::CleanupOldCrashReports(
    std::vector<CrashReportDatabase::Report>& sorted_reports) {
  // Cleanup the oldest reports if we have too many in the database.
  size_t num_reports = sorted_reports.size();
  if (num_reports > kMaxReportsToRetain) {
    HOST_LOG << "Too many crash reports in database. Retaining most recent "
             << kMaxReportsToRetain;
    for (size_t i = kMaxReportsToRetain; i < num_reports; ++i) {
      const auto& report = sorted_reports[i];
      HOST_LOG << "  Deleting crash report: " << report.id << " ("
               << report.uuid.ToString() << ") "
               << base::Time::FromTimeT(report.creation_time);
      auto status = database_->DeleteReport(report.uuid);
      if (status != CrashReportDatabase::OperationStatus::kNoError) {
        LOG(ERROR) << "  Unable to delete crash report: " << status << " "
                   << report.id << " (" << report.uuid.ToString() << ")";
      }
    }
    sorted_reports.resize(kMaxReportsToRetain);
  }

  base::Time now = base::Time::Now();
  base::Time threshold = now - base::Days(kMaxReportAgeDays);

  // Cleanup old uploaded reports.
  bool header_shown = false;
  for (const auto& report : sorted_reports) {
    base::Time created = base::Time::FromTimeT(report.creation_time);
    if (report.uploaded && created < threshold) {
      if (!header_shown) {
        header_shown = true;
        HOST_LOG << "Deleting uploaded crash reports older than "
                 << kMaxReportAgeDays << " days:";
      }
      HOST_LOG << "  Deleting crash report: " << report.id << " (" << created
               << ")";
      auto status = database_->DeleteReport(report.uuid);
      if (status != CrashReportDatabase::OperationStatus::kNoError) {
        LOG(ERROR) << "  Unable to delete uploaded crash report: " << status
                   << " " << report.id << " (" << report.uuid.ToString() << ")";
      }
    }
  }

  // Cleanup old reports that haven't been uploaded.
  header_shown = false;
  for (const auto& report : sorted_reports) {
    base::Time created = base::Time::FromTimeT(report.creation_time);
    if (!report.uploaded && created < threshold) {
      if (!header_shown) {
        header_shown = true;
        HOST_LOG << "Deleting crash reports older than " << kMaxReportAgeDays
                 << " days:";
      }
      // We need to log |uuid| here because only uploaded reports have an |id|.
      HOST_LOG << "  Deleting crash report: " << report.uuid.ToString() << " ("
               << created << ")";
      auto status = database_->DeleteReport(report.uuid);
      if (status != CrashReportDatabase::OperationStatus::kNoError) {
        LOG(ERROR) << "  Unable to delete old crash report: " << status << " ("
                   << report.uuid.ToString() << ")";
      }
    }
  }
}

bool CrashpadLinux::InitializeCrashpadDatabase(base::FilePath database_path) {
  base::File::Error error;
  if (!base::CreateDirectoryAndGetError(database_path, &error)) {
    LOG(ERROR) << "Unable to create crashpad database directory: " << error;
    return false;
  }
  database_ = CrashReportDatabase::Initialize(database_path);
  if (!database_) {
    LOG(ERROR) << "Failed to initialize database for Crashpad at "
               << database_path.value();
    return false;
  }

  return true;
}

bool CrashpadLinux::Initialize() {
  base::FilePath handler_path;
  if (!GetCrashpadHandlerPath(&handler_path)) {
    return false;
  }

  base::FilePath database_path = GetCrashpadDatabasePath();
  if (!InitializeCrashpadDatabase(database_path)) {
    return false;
  }

  // We only initialize crash handling if the user has consented to record and
  // upload reports, so we can simply enable it here.
  if (!database_->GetSettings()->SetUploadsEnabled(true)) {
    LOG(WARNING) << "Unable to enable Crashpad uploads.";
  }

  // Leave metrics_path empty because this option is not used (or supported) on
  // non-Chromium builds.
  base::FilePath metrics_path;

  std::map<std::string, std::string> annotations;
  annotations["prod"] = "Chromoting_Linux";
  annotations["ver"] = REMOTING_VERSION_STRING;
  annotations["plat"] = std::string("Linux");

  std::vector<std::string> arguments;
  // Make sure Crashpad's generate_dump tool includes monitor-self annotations.
  // This creates a second crashpad instance that monitors the handler so it can
  // report crashes in the handler.
  arguments.push_back("--monitor-self-annotation=ptype=crashpad-handler");

  crashpad::CrashpadClient client;
  if (!client.StartHandler(handler_path, database_path, metrics_path,
                           kDefaultCrashpadUploadUrl, annotations, arguments,
                           false, false)) {
    LOG(ERROR) << "Failed to start Crashpad handler.";
    return false;
  }

  HOST_LOG << "Crashpad handler started.";
  return true;
}

void CrashpadLinux::LogAndCleanupCrashpadDatabase() {
  if (!database_) {
    return;
  }

  CrashReportDatabase::OperationStatus status;
  std::vector<CrashReportDatabase::Report> completed_reports;
  status = database_->GetCompletedReports(&completed_reports);
  if (status == CrashReportDatabase::OperationStatus::kNoError) {
    SortAndLogCrashReports(completed_reports, "Completed");
    CleanupOldCrashReports(completed_reports);
  } else {
    LOG(ERROR) << "Unable to read completed crash reports: " << status;
  }

  std::vector<CrashReportDatabase::Report> pending_reports;
  status = database_->GetPendingReports(&pending_reports);
  if (status == CrashReportDatabase::OperationStatus::kNoError) {
    SortAndLogCrashReports(pending_reports, "Pending");
  } else {
    LOG(ERROR) << "Unable to read pending crash reports: " << status;
  }
}

// static
CrashpadLinux& CrashpadLinux::GetInstance() {
  static base::NoDestructor<CrashpadLinux> instance;
  return *instance;
}

}  // namespace remoting
