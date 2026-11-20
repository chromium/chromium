// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_CRASH_CRASHPAD_DATABASE_MANAGER_H_
#define REMOTING_BASE_CRASH_CRASHPAD_DATABASE_MANAGER_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/raw_ref.h"
#include "build/build_config.h"
#include "third_party/crashpad/crashpad/client/crash_report_database.h"

namespace remoting {

// Maximum length (excluding trailing NUL) of a Crashpad upload URL override,
// matching base::win::RegKey::ReadValue's 1024-wchar limit on Windows.
inline constexpr size_t kMaxCrashpadUploadUrlChars = 1023;

#if BUILDFLAG(IS_WIN)
// Returns true if the current process token's user SID is LocalSystem.
bool IsCurrentProcessLocalSystem();
#endif  // BUILDFLAG(IS_WIN)

base::FilePath GetCrashpadDatabasePath();

// Validates an admin-configured Crashpad upload URL override. Enforces a
// strict subset of URLs accepted by WinHttpCrackUrl, libcurl, and
// crashpad::CrackURL(): at most `kMaxCrashpadUploadUrlChars` ASCII characters,
// lowercase http:// or https:// scheme, ASCII hostname or IPv4 address (URL
// userinfo and IPv6 literals are not supported), optional numeric port, and a
// '/' resource path without a '#' fragment (CrashReportUploadThread appends
// query parameters to the end of the URL and http_transport_win.cc drops
// everything from '#' onward). Plain HTTP is restricted to loopback
// (127.0.0.1 or localhost) so minidumps cannot be sent in cleartext over a
// network.
bool IsValidCrashpadUploadUrlOverride(std::string_view url);

class CrashpadDatabaseManager {
 public:
  class Logger {
   public:
    virtual ~Logger() = default;

    virtual void Log(std::string_view message) const = 0;
    virtual void LogError(std::string_view message) const = 0;
  };

  explicit CrashpadDatabaseManager(Logger& logger);

  CrashpadDatabaseManager(const CrashpadDatabaseManager&) = delete;
  CrashpadDatabaseManager& operator=(const CrashpadDatabaseManager&) = delete;

  ~CrashpadDatabaseManager();

  // Initializes the database located at `database_path`. If `database_path` is
  // empty, falls back to `GetCrashpadDatabasePath()`.
  bool InitializeCrashpadDatabase(
      const base::FilePath& database_path = base::FilePath());
  // Opens an existing Crashpad database at `database_path` (or
  // `GetCrashpadDatabasePath()` if empty) without creating its root directory.
  bool InitializeCrashpadDatabaseWithoutCreating(
      const base::FilePath& database_path = base::FilePath());
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
