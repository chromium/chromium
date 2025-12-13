// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/crash/crashpad_linux.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/time/time.h"
#include "remoting/base/crash/crashpad_database_manager.h"
#include "remoting/base/file_path_util_linux.h"
#include "remoting/base/logging.h"
#include "remoting/base/version.h"
#include "third_party/crashpad/crashpad/client/crash_report_database.h"
#include "third_party/crashpad/crashpad/client/crashpad_client.h"

namespace remoting {

constexpr char kChromotingCrashpadHandler[] = "crashpad-handler";
constexpr char kDefaultCrashpadUploadUrl[] =
    "https://clients2.google.com/cr/report";

CrashpadLinux::CrashpadLinux() : database_(*this) {}

bool CrashpadLinux::Initialize() {
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
  annotations["prod"] = "Chromoting_Linux";
  annotations["ver"] = REMOTING_VERSION_STRING;
  annotations["plat"] = std::string("Linux");

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

void CrashpadLinux::LogAndCleanupCrashpadDatabase() {
  database_.LogCompletedCrashpadReports();
  database_.LogPendingCrashpadReports();
  database_.CleanupCompletedCrashpadReports();
}

// static
CrashpadLinux& CrashpadLinux::GetInstance() {
  static base::NoDestructor<CrashpadLinux> instance;
  return *instance;
}

// private

// CrashpadDatabaseManager::Logger overrides
void CrashpadLinux::Log(const std::string message) const {
  HOST_LOG << message;
}

void CrashpadLinux::LogError(const std::string message) const {
  LOG(ERROR) << message;
}

bool CrashpadLinux::GetCrashpadHandlerPath(base::FilePath* handler_path) {
  if (!base::PathService::Get(base::DIR_EXE, handler_path)) {
    LOG(ERROR) << "Unable to get exe dir for crashpad handler";
    return false;
  }
  *handler_path = handler_path->Append(kChromotingCrashpadHandler);
  return true;
}

}  // namespace remoting
