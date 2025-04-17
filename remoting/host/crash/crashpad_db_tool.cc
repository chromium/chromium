// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <vector>

#include "base/files/file_path.h"
#include "remoting/base/crash/crashpad_database_manager.h"
#include "remoting/base/file_path_util_linux.h"

namespace remoting {

class RemotingCrashpadDatabaseTool : CrashpadDatabaseManager::Logger {
 public:
  RemotingCrashpadDatabaseTool();

  int DatabaseToolMain();

 private:
  // CrashpadDatabaseManager::Logger overrides
  void Log(std::string message) const override;
  void LogError(std::string message) const override;

  remoting::CrashpadDatabaseManager database_;
};

RemotingCrashpadDatabaseTool::RemotingCrashpadDatabaseTool()
    : database_(*this) {}

// CrashpadDatabaseManager::Logger overrides
void RemotingCrashpadDatabaseTool::Log(const std::string message) const {
  std::cout << message << '\n';
}

void RemotingCrashpadDatabaseTool::LogError(const std::string message) const {
  std::cerr << message << '\n';
}

int RemotingCrashpadDatabaseTool::DatabaseToolMain() {
  if (!database_.InitializeCrashpadDatabase()) {
    std::cout << "Unable to load crashpad database at "
              << GetCrashpadDatabasePath() << '\n';
    return EXIT_FAILURE;
  }

  database_.LogCompletedCrashpadReports();
  return EXIT_SUCCESS;
}

}  // namespace remoting

int main(int argc, char* argv[]) {
  return remoting::RemotingCrashpadDatabaseTool().DatabaseToolMain();
}
