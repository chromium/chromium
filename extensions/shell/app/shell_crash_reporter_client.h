// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_APP_SHELL_CRASH_REPORTER_CLIENT_H_
#define EXTENSIONS_SHELL_APP_SHELL_CRASH_REPORTER_CLIENT_H_

#include "components/crash/core/app/crash_reporter_client.h"

namespace extensions {

class ShellCrashReporterClient : public crash_reporter::CrashReporterClient {
 public:
  ShellCrashReporterClient();

  ShellCrashReporterClient(const ShellCrashReporterClient&) = delete;
  ShellCrashReporterClient& operator=(const ShellCrashReporterClient&) = delete;

  ~ShellCrashReporterClient() override;

  // crash_reporter::CrashReporterClient:
  void SetCrashReporterClientIdFromGUID(
      const std::string& client_guid) override;
  void GetProductNameAndVersion(const char** product_name,
                                const char** version) override;
  base::FilePath GetReporterLogFilename() override;
  bool GetCrashDumpLocation(base::FilePath* crash_dir) override;
  bool IsRunningUnattended() override;
  bool GetCollectStatsConsent() override;
  bool EnableBreakpadForProcess(const std::string& process_type) override;
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_APP_SHELL_CRASH_REPORTER_CLIENT_H_
