// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/crash/crashpad.h"

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

namespace {

using crashpad::CrashReportDatabase;

constexpr char kChromotingCrashpadHandler[] = "crashpad-handler";
constexpr char kDefaultCrashpadUploadUrl[] =
    "https://clients2.google.com/cr/report";
const base::FilePath::CharType kChromotingCrashpadDatabasePath[] =
    FILE_PATH_LITERAL("crashpad");

class CrashpadLinux {
 public:
  CrashpadLinux();

  CrashpadLinux(const CrashpadLinux&) = delete;
  CrashpadLinux& operator=(const CrashpadLinux&) = delete;

  ~CrashpadLinux() = delete;

  bool Initialize();

  static CrashpadLinux& GetInstance();

 private:
  bool GetCrashpadHandlerPath(base::FilePath* handler_path);
  base::FilePath GetCrashpadDatabasePath();
  void LogCrashReportInfo(const CrashReportDatabase::Report& report);
  bool InitializeCrashpadDatabase(base::FilePath database_path);

  std::unique_ptr<CrashReportDatabase> database_;
};

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
  HOST_LOG << "  Crash id: " << report.id;
  HOST_LOG << "    path: " << report.file_path;
  HOST_LOG << "    uuid: " << report.uuid.ToString();
  HOST_LOG << "    created: " << base::Time::FromTimeT(report.creation_time);
  HOST_LOG << "    uploaded: " << (report.uploaded ? "yes" : "no")
           << " (attempts: " << report.upload_attempts << ")";
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

  // Log the crash report ids.
  CrashReportDatabase::OperationStatus status;
  std::vector<CrashReportDatabase::Report> completed_reports;
  status = database_->GetCompletedReports(&completed_reports);
  if (status == CrashReportDatabase::OperationStatus::kNoError) {
    HOST_LOG << "Completed crash reports: " << completed_reports.size();
    for (const auto& report : completed_reports) {
      LogCrashReportInfo(report);
    }
  } else {
    LOG(ERROR) << "Unable to read completed crash reports: " << status;
  }

  std::vector<CrashReportDatabase::Report> pending_reports;
  status = database_->GetPendingReports(&pending_reports);
  if (status == CrashReportDatabase::OperationStatus::kNoError) {
    HOST_LOG << "Pending crash reports: " << pending_reports.size();
    for (const auto& report : pending_reports) {
      LogCrashReportInfo(report);
    }
  } else {
    LOG(ERROR) << "Unable to read pending crash reports: " << status;
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

// static
CrashpadLinux& CrashpadLinux::GetInstance() {
  static base::NoDestructor<CrashpadLinux> instance;
  return *instance;
}

}  // namespace

void InitializeCrashReporting() {
  // Touch the object to make sure it is initialized.
  CrashpadLinux::GetInstance().Initialize();
}

}  // namespace remoting
