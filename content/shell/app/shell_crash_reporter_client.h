// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_APP_SHELL_CRASH_REPORTER_CLIENT_H_
#define CONTENT_SHELL_APP_SHELL_CRASH_REPORTER_CLIENT_H_

#include "build/build_config.h"
#include "components/crash/core/app/crash_reporter_client.h"

namespace content {

class ShellCrashReporterClient : public crash_reporter::CrashReporterClient {
 public:
  ShellCrashReporterClient();

  ShellCrashReporterClient(const ShellCrashReporterClient&) = delete;
  ShellCrashReporterClient& operator=(const ShellCrashReporterClient&) = delete;

  ~ShellCrashReporterClient() override;

#if BUILDFLAG(IS_WIN)
  // Returns a textual description of the product type and version to include
  // in the crash report.
  void GetProductNameAndVersion(const std::wstring& exe_path,
                                std::wstring* product_name,
                                std::wstring* version,
                                std::wstring* special_build,
                                std::wstring* channel_name) override;
#endif

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)
  // Returns a textual description of the product type and version to include
  // in the crash report.
  void GetProductNameAndVersion(const char** product_name,
                                const char** version) override;
  void GetProductNameAndVersion(std::string* product_name,
                                std::string* version,
                                std::string* channel) override;
  base::FilePath GetReporterLogFilename() override;
#endif

  // The location where minidump files should be written. Returns true if
  // |crash_dir| was set.
#if BUILDFLAG(IS_WIN)
  bool GetCrashDumpLocation(std::wstring* crash_dir) override;
#else
  bool GetCrashDumpLocation(base::FilePath* crash_dir) override;
#endif

#if BUILDFLAG(IS_ANDROID)
  // Returns the descriptor key of the android minidump global descriptor.
  int GetAndroidMinidumpDescriptor() override;
#endif

  bool EnableBreakpadForProcess(const std::string& process_type) override;
};

}  // namespace content

#endif  // CONTENT_SHELL_APP_SHELL_CRASH_REPORTER_CLIENT_H_
