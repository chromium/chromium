// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/crash/crashpad_database_manager.h"

#include <stdlib.h>

#include <vector>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "remoting/base/branding.h"
#include "third_party/crashpad/crashpad/client/crash_report_database.h"
#include "third_party/crashpad/crashpad/client/settings.h"

#if BUILDFLAG(IS_WIN)
#include "base/base_paths.h"
#include "base/strings/utf_string_conversions.h"
#elif BUILDFLAG(IS_LINUX)
#include <sys/types.h>
#include <unistd.h>

#include "base/environment.h"
#include "remoting/base/file_path_util_linux.h"
#endif  // BUILDFLAG(IS_WIN)

namespace remoting {
namespace {

#if !BUILDFLAG(IS_LINUX)
const base::FilePath::CharType kChromotingCrashpadDatabasePath[] =
    FILE_PATH_LITERAL("crashpad");
#endif

// Maximum number of crash reports to log. Reports are sorted by timestamp so
// the most recent N reports will be logged.
const size_t kMaxReportsToLog = 2;

// Maximum number of crash reports to retain in the database. When the database
// contains more than this number, the oldest ones will be deleted, regardless
// of `kMaxReportAgeDays`.
const size_t kMaxReportsToRetain = 20;

// Maximum number of days to keep reports around in the local database.
const size_t kMaxReportAgeDays = 7;

#if BUILDFLAG(IS_LINUX)
inline base::FilePath GetUnifiedCrashpadDatabasePath() {
  return GetVarLibDir().Append("crashpad");
}
#endif  // BUILDFLAG(IS_LINUX)

// Formats a `base::Time` as a UTC string ("YYYY-MM-DD HH:MM:SS UTC").
// We intentionally avoid `base::TimeFormatHTTP()` (and `//base:i18n` in
// general) because `remoting_crashpad_handler` is a minimal, standalone binary
// that does not initialize ICU or package `icudtl.dat`. Using ICU without
// initialization would cause the handler to crash.
std::string FormatTime(base::Time time) {
  base::Time::Exploded exploded;
  time.UTCExplode(&exploded);
  return base::StringPrintf("%04d-%02d-%02d %02d:%02d:%02d UTC", exploded.year,
                            exploded.month, exploded.day_of_month,
                            exploded.hour, exploded.minute, exploded.second);
}

}  // namespace

base::FilePath GetCrashpadDatabasePath() {
  static const base::NoDestructor<base::FilePath> database_path([]() {
#if BUILDFLAG(IS_WIN)
    base::FilePath path;
    base::PathService::Get(base::BasePathKey::DIR_ASSETS, &path);
    return path.Append(kChromotingCrashpadDatabasePath);
#elif BUILDFLAG(IS_LINUX)
    if (getuid() == 0) {
      // Used by the daemon process or the elevated start-host process.
      return GetUnifiedCrashpadDatabasePath();
    }
    // 4-tier cascading resolution for non-root processes (e.g. single-process
    // ME2ME host, IT2ME Native Messaging Host).
    std::optional<std::string> xdg_runtime_dir =
        base::Environment::Create()->GetVar("XDG_RUNTIME_DIR");
    if (!xdg_runtime_dir.has_value() || xdg_runtime_dir->empty()) {
      // $XDG_RUNTIME_DIR may not be available yet on the desktop process (since
      // it is loaded from systemd in DesktopProcessMain()), but for modern
      // Linux systems using systemd, it is always /run/user/<uid>.
      // TODO: crbug.com/475611769 - Make DesktopProcessMain() execve() itself
      // so that the environment variable is available here.
      xdg_runtime_dir = base::StringPrintf("/run/user/%u", getuid());
    }
    base::FilePath xdg_runtime_dir_path(*xdg_runtime_dir);
    if (base::DirectoryExists(xdg_runtime_dir_path)) {
      return xdg_runtime_dir_path.Append("crd_crashpad");
    }

    base::FilePath config_dir = GetConfigDir();
    if (!config_dir.empty()) {
      return config_dir.Append("crashpad");
    }

    // Fallback to a unique secure temporary directory if XDG_RUNTIME_DIR is
    // missing. This is critical to support the early start-host flow before the
    // user has logged in. Note: Utilizing a dynamic temporary directory per
    // process session breaks crash report persistence across process restarts,
    // preventing uploads of previous unhandled crashes. This is an accepted
    // trade-off to eliminate multi-user collision DoS risks and ensure absolute
    // protection against CWE-377 pre-creation symlink traversal attacks in
    // shared /tmp.
    base::FilePath temp_dir;
    if (base::GetTempDir(&temp_dir)) {
      base::FilePath unique_temp_dir;
      if (base::CreateTemporaryDirInDir(
              temp_dir, /*prefix=*/FILE_PATH_LITERAL("crd_crashpad."),
              &unique_temp_dir)) {
        return unique_temp_dir;
      }
    }

    return base::FilePath();
#else
    return GetConfigDir().Append(kChromotingCrashpadDatabasePath);
#endif
  }());
  return *database_path;
}

CrashpadDatabaseManager::CrashpadDatabaseManager(Logger& logger)
    : logger_(logger) {}

CrashpadDatabaseManager::~CrashpadDatabaseManager() = default;

bool CrashpadDatabaseManager::InitializeCrashpadDatabase(
    const base::FilePath& database_path) {
  base::FilePath target_database_path =
      database_path.empty() ? GetCrashpadDatabasePath() : database_path;
  base::File::Error error;
  if (!base::CreateDirectoryAndGetError(target_database_path, &error)) {
#if BUILDFLAG(IS_WIN)
    logger_->LogError("Unable to get directory for crash database: " +
                      base::WideToUTF8(target_database_path.value()));
#else
    logger_->LogError("Unable to get directory for crash database: " +
                      target_database_path.value());
#endif
    logger_->LogError("File Error: " + base::File::ErrorToString(error));
    return false;
  }
  database_ = crashpad::CrashReportDatabase::Initialize(target_database_path);
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
  completed_reports_.clear();
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
  pending_reports_.clear();
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
  // `id` will only be assigned if the report has been successfully uploaded.
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
               FormatTime(base::Time::FromTimeT(report.creation_time)));
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
          FormatTime(base::Time::FromTimeT(report.creation_time));
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
                   FormatTime(created) + ")");
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
      // We need to log `uuid` here because only uploaded reports have an `id`.
      logger_->Log("  Deleting crash report: " + report.uuid.ToString() + " (" +
                   FormatTime(created) + ")");
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
