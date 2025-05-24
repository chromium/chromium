// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/crash/crashpad_database_manager.h"

#include <stdlib.h>

#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/i18n/time_formatting.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "remoting/base/file_path_util_linux.h"
#include "third_party/crashpad/crashpad/client/crash_report_database.h"
#include "third_party/crashpad/crashpad/client/settings.h"

#if BUILDFLAG(IS_WIN)
#include "base/base_paths.h"
#include "base/strings/utf_string_conversions.h"
#endif  // BUILDFLAG(IS_WIN)

namespace {

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

}  // namespace

namespace remoting {

base::FilePath GetCrashpadDatabasePath() {
  base::FilePath database_path;
#if BUILDFLAG(IS_WIN)
  base::PathService::Get(base::BasePathKey::DIR_ASSETS, &database_path);
#else
  database_path = GetConfigDirectoryPath();
#endif
  return database_path.Append(kChromotingCrashpadDatabasePath);
}

CrashpadDatabaseManager::CrashpadDatabaseManager(Logger& logger)
    : logger_(logger) {}

CrashpadDatabaseManager::~CrashpadDatabaseManager() = default;

bool CrashpadDatabaseManager::InitializeCrashpadDatabase() {
  base::FilePath database_path = GetCrashpadDatabasePath();
  base::File::Error error;
  if (!base::CreateDirectoryAndGetError(database_path, &error)) {
#if BUILDFLAG(IS_WIN)
    logger_->LogError("Unable to get directory for crash database: " +
                      base::WideToUTF8(database_path.value()));
#else
    logger_->LogError("Unable to get directory for crash database: " +
                      database_path.value());
#endif
    logger_->LogError("File Error: " + base::File::ErrorToString(error));
    return false;
  }
  database_ = crashpad::CrashReportDatabase::Initialize(database_path);
  if (!database_) {
    logger_->LogError("Unable to initialize crash database");
    return false;
  }

  // Load and sort the completed crash reports.
  if (!LoadCompletedReports()) {
    return false;
  }
  if (!LoadPendingReports()) {
    return false;
  }

  return true;
}

bool CrashpadDatabaseManager::EnableReportUploads() {
  if (!database_ || !database_->GetSettings()->SetUploadsEnabled(true)) {
    logger_->LogError("Unable to enable Crashpad report uploads");
    return false;
  }
  return true;
}

void CrashpadDatabaseManager::LogCompletedCrashpadReports() {
  if (!database_) {
    logger_->LogError("Crashpad database has not been initialized");
    return;
  }

  LogCrashReports(completed_reports_, "Completed");
}

void CrashpadDatabaseManager::LogPendingCrashpadReports() {
  if (!database_) {
    logger_->LogError("Crashpad database has not been initialized");
    return;
  }

  LogCrashReports(pending_reports_, "Pending");
}

bool CrashpadDatabaseManager::CleanupCompletedCrashpadReports() {
  if (!database_) {
    logger_->LogError("Crashpad database has not been initialized");
    return false;
  }

  CleanupCrashReports(completed_reports_);
  // Reload the completed reports since we may have deleted some entries.
  return LoadCompletedReports();
}

// private

bool CrashpadDatabaseManager::LoadCompletedReports() {
  crashpad::CrashReportDatabase::OperationStatus status =
      database_->GetCompletedReports(&completed_reports_);
  if (status != crashpad::CrashReportDatabase::OperationStatus::kNoError) {
    logger_->LogError("Unable to read completed crash reports: " +
                      base::NumberToString(status));
    return false;
  }
  SortCrashReports(completed_reports_);
  return true;
}

bool CrashpadDatabaseManager::LoadPendingReports() {
  crashpad::CrashReportDatabase::OperationStatus status =
      database_->GetPendingReports(&pending_reports_);
  if (status != crashpad::CrashReportDatabase::OperationStatus::kNoError) {
    logger_->LogError("Unable to read pending crash reports: " +
                      base::NumberToString(status));
    return false;
  }
  SortCrashReports(pending_reports_);
  return true;
}

void CrashpadDatabaseManager::SortCrashReports(
    std::vector<crashpad::CrashReportDatabase::Report>& reports) {
  std::sort(reports.begin(), reports.end(),
            [](crashpad::CrashReportDatabase::Report const& a,
               crashpad::CrashReportDatabase::Report const& b) {
              return a.creation_time > b.creation_time;
            });
}

void CrashpadDatabaseManager::LogCrashReportInfo(
    const crashpad::CrashReportDatabase::Report& report) {
  const std::string& id = report.id;
  // |id| will only be assigned if the report has been successfully uploaded.
  if (id.empty()) {
    logger_->Log("  Crash id: <unassigned>");
  } else {
    logger_->Log("  Crash id: " + id + " (http://go/crash/" + id + ")");
  }
#if BUILDFLAG(IS_WIN)
  logger_->Log("    path: " + base::WideToUTF8(report.file_path.value()));
#else
  logger_->Log("    path: " + report.file_path.value());
#endif
  logger_->Log("    uuid: " + report.uuid.ToString());
  logger_->Log("    created: " +
               TimeFormatHTTP(base::Time::FromTimeT(report.creation_time)));
  std::string uploaded = report.uploaded ? "yes" : "no";
  logger_->Log("    uploaded: " + uploaded + " (attempts: " +
               base::NumberToString(report.upload_attempts) + ")");
}

void CrashpadDatabaseManager::LogCrashReports(
    std::vector<crashpad::CrashReportDatabase::Report>& reports,
    std::string_view report_type) {
  size_t num_reports = reports.size();
  if (num_reports > kMaxReportsToLog) {
    logger_->Log("Recent " + std::string(report_type) + " crash reports: " +
                 base::NumberToString(num_reports) + " (most recent " +
                 base::NumberToString(kMaxReportsToLog) + " shown)");
  } else {
    logger_->Log("Recent " + std::string(report_type) +
                 " crash reports: " + base::NumberToString(num_reports));
  }

  for (size_t i = 0; i < num_reports && i < kMaxReportsToLog; ++i) {
    LogCrashReportInfo(reports[i]);
  }
}

bool CrashpadDatabaseManager::CleanupCrashReports(
    std::vector<crashpad::CrashReportDatabase::Report>& reports) {
  if (!database_) {
    return false;
  }

  // Cleanup the oldest reports if we have too many in the database.
  size_t num_reports = reports.size();
  if (num_reports > kMaxReportsToRetain) {
    logger_->Log("Too many crash reports in database. Retaining most recent " +
                 base::NumberToString(kMaxReportsToRetain));
    for (size_t i = kMaxReportsToRetain; i < num_reports; ++i) {
      const auto& report = reports[i];
      std::string creation_time =
          base::TimeFormatHTTP(base::Time::FromTimeT(report.creation_time));
      logger_->Log("  Deleting crash report: " + report.id + " (" +
                   report.uuid.ToString() + ") " + creation_time);
      auto status = database_->DeleteReport(report.uuid);
      if (status != crashpad::CrashReportDatabase::OperationStatus::kNoError) {
        logger_->LogError(
            "  Unable to delete crash report: " + base::NumberToString(status) +
            " " + report.id + " (" + report.uuid.ToString() + ")");
        return false;
      }
    }
    reports.resize(kMaxReportsToRetain);
  }

  base::Time now = base::Time::Now();
  base::Time threshold = now - base::Days(kMaxReportAgeDays);

  // Cleanup old uploaded reports.
  bool header_shown = false;
  for (const auto& report : reports) {
    base::Time created = base::Time::FromTimeT(report.creation_time);
    if (report.uploaded && created < threshold) {
      if (!header_shown) {
        header_shown = true;
        logger_->Log("Deleting uploaded crash reports older than " +
                     base::NumberToString(kMaxReportAgeDays) + " days:");
      }
      logger_->Log("  Deleting crash report: " + report.id + " (" +
                   base::TimeFormatHTTP(created) + ")");
      auto status = database_->DeleteReport(report.uuid);
      if (status != crashpad::CrashReportDatabase::OperationStatus::kNoError) {
        logger_->LogError("  Unable to delete uploaded crash report: " +
                          base::NumberToString(status) + " " + report.id +
                          " (" + report.uuid.ToString() + ")");
        return false;
      }
    }
  }

  // Cleanup old reports that haven't been uploaded.
  header_shown = false;
  for (const auto& report : reports) {
    base::Time created = base::Time::FromTimeT(report.creation_time);
    if (!report.uploaded && created < threshold) {
      if (!header_shown) {
        header_shown = true;
        logger_->Log("Deleting crash reports older than " +
                     base::NumberToString(kMaxReportAgeDays) + " days:");
      }
      // We need to log |uuid| here because only uploaded reports have an |id|.
      logger_->Log("  Deleting crash report: " + report.uuid.ToString() + " (" +
                   TimeFormatHTTP(created) + ")");
      auto status = database_->DeleteReport(report.uuid);
      if (status != crashpad::CrashReportDatabase::OperationStatus::kNoError) {
        logger_->LogError("  Unable to delete old crash report: " +
                          base::NumberToString(status) + " (" +
                          report.uuid.ToString() + ")");
        return false;
      }
    }
  }
  return true;
}

}  // namespace remoting
