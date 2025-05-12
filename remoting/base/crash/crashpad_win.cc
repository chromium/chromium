// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/crash/crashpad_win.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/time/time.h"
#include "remoting/base/crash/crashpad_database_manager.h"
#include "remoting/base/logging.h"
#include "remoting/base/version.h"
#include "third_party/crashpad/crashpad/client/crash_report_database.h"
#include "third_party/crashpad/crashpad/client/crashpad_client.h"

namespace remoting {

constexpr wchar_t kChromotingCrashpadHandler[] =
    L"remoting_crashpad_handler.exe";
constexpr char kDefaultCrashpadUploadUrl[] =
    "https://clients2.google.com/cr/report";

CrashpadWin::CrashpadWin() : database_(*this) {}

bool CrashpadWin::Initialize() {
  if (!database_.InitializeCrashpadDatabase()) {
    LOG(ERROR) << "Failed to initialize database for Crashpad";
    return false;
  }

  // We only initialize crash handling if the user has consented to record and
  // upload reports, so we can simply enable it here.
  if (!database_.EnableReportUploads()) {
    LOG(WARNING) << "Unable to enable Crashpad uploads.";
  }

  // Leave metrics_path empty because this option is not used (or supported) on
  // non-Chromium builds.
  base::FilePath metrics_path;

  std::map<std::string, std::string> annotations;
  annotations["prod"] = "Chromoting_Win";
  annotations["ver"] = REMOTING_VERSION_STRING;
  annotations["plat"] = std::string("Windows");

  std::vector<std::string> arguments;
  // Make sure Crashpad's generate_dump tool includes monitor-self annotations.
  // This creates a second crashpad instance that monitors the handler so it can
  // report crashes in the handler.
  arguments.push_back("--monitor-self-annotation=ptype=crashpad-handler");

  base::FilePath handler_path;
  if (!GetCrashpadHandlerPath(&handler_path)) {
    return false;
  }

  crashpad::CrashpadClient client;
  if (!client.StartHandler(handler_path, GetCrashpadDatabasePath(),
                           metrics_path, kDefaultCrashpadUploadUrl, annotations,
                           arguments, false, false)) {
    LOG(ERROR) << "Failed to start Crashpad handler.";
    return false;
  }

  HOST_LOG << "Crashpad handler started.";
  return true;
}

void CrashpadWin::LogAndCleanupCrashpadDatabase() {
  database_.LogCompletedCrashpadReports();
  database_.LogPendingCrashpadReports();
  database_.CleanupCompletedCrashpadReports();
}

// static
CrashpadWin& CrashpadWin::GetInstance() {
  static base::NoDestructor<CrashpadWin> instance;
  return *instance;
}

// private

// CrashpadDatabaseManager::Logger overrides
void CrashpadWin::Log(const std::string message) const {
  HOST_LOG << message;
}

void CrashpadWin::LogError(const std::string message) const {
  LOG(ERROR) << message;
}

bool CrashpadWin::GetCrashpadHandlerPath(base::FilePath* handler_path) {
  if (!base::PathService::Get(base::DIR_EXE, handler_path)) {
    LOG(ERROR) << "Unable to get exe dir for crashpad handler";
    return false;
  }
  *handler_path = handler_path->Append(kChromotingCrashpadHandler);
  return true;
}

}  // namespace remoting
