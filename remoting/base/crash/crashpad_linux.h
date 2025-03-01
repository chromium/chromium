// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_CRASH_CRASHPAD_LINUX_H_
#define REMOTING_BASE_CRASH_CRASHPAD_LINUX_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "third_party/crashpad/crashpad/client/crash_report_database.h"
#include "third_party/crashpad/crashpad/client/crashpad_client.h"

namespace remoting {

class CrashpadLinux {
 public:
  CrashpadLinux();

  CrashpadLinux(const CrashpadLinux&) = delete;
  CrashpadLinux& operator=(const CrashpadLinux&) = delete;

  ~CrashpadLinux() = delete;

  bool Initialize();
  void LogAndCleanupCrashpadDatabase();

  static CrashpadLinux& GetInstance();

 private:
  bool GetCrashpadHandlerPath(base::FilePath* handler_path);
  base::FilePath GetCrashpadDatabasePath();
  void LogCrashReportInfo(const crashpad::CrashReportDatabase::Report& report);
  void SortAndLogCrashReports(
      std::vector<crashpad::CrashReportDatabase::Report>& reports,
      std::string report_type);
  void CleanupOldCrashReports(
      std::vector<crashpad::CrashReportDatabase::Report>& sorted_reports);

  bool InitializeCrashpadDatabase(base::FilePath database_path);

  std::unique_ptr<crashpad::CrashReportDatabase> database_;
};

}  // namespace remoting

#endif  // REMOTING_BASE_CRASH_CRASHPAD_LINUX_H_
