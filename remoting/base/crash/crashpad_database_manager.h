// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_CRASH_CRASHPAD_DATABASE_MANAGER_H_
#define REMOTING_BASE_CRASH_CRASHPAD_DATABASE_MANAGER_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/raw_ref.h"
#include "third_party/crashpad/crashpad/client/crash_report_database.h"

namespace remoting {

base::FilePath GetCrashpadDatabasePath();

class CrashpadDatabaseManager {
 public:
  class Logger {
   public:
    virtual void Log(const std::string message) const = 0;
    virtual void LogError(const std::string message) const = 0;
  };

  explicit CrashpadDatabaseManager(Logger& logger);

  CrashpadDatabaseManager(const CrashpadDatabaseManager&) = delete;
  CrashpadDatabaseManager& operator=(const CrashpadDatabaseManager&) = delete;

  ~CrashpadDatabaseManager();

  bool InitializeCrashpadDatabase();
  bool EnableReportUploads();

  void LogCompletedCrashpadReports();
  void LogPendingCrashpadReports();
  bool CleanupCompletedCrashpadReports();

 private:
  bool LoadCompletedReports();
  bool LoadPendingReports();
  void SortCrashReports(
      std::vector<crashpad::CrashReportDatabase::Report>& reports);

  void LogCrashReportInfo(const crashpad::CrashReportDatabase::Report& report);
  void LogCrashReports(
      std::vector<crashpad::CrashReportDatabase::Report>& reports,
      std::string_view report_type);

  bool CleanupCrashReports(
      std::vector<crashpad::CrashReportDatabase::Report>& reports);

  const raw_ref<Logger> logger_;

  std::unique_ptr<crashpad::CrashReportDatabase> database_;

  std::vector<crashpad::CrashReportDatabase::Report> completed_reports_;
  std::vector<crashpad::CrashReportDatabase::Report> pending_reports_;
};

}  // namespace remoting

#endif  // REMOTING_BASE_CRASH_CRASHPAD_DATABASE_MANAGER_H_
