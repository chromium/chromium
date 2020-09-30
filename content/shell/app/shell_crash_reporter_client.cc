// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/app/shell_crash_reporter_client.h"

#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/public/common/content_switches.h"
#include "content/shell/common/shell_switches.h"

#if defined(OS_ANDROID)
#include "content/shell/android/shell_descriptors.h"
#endif

namespace content {

ShellCrashReporterClient::ShellCrashReporterClient() {}
ShellCrashReporterClient::~ShellCrashReporterClient() {}

#if defined(OS_WIN)
void ShellCrashReporterClient::GetProductNameAndVersion(
    const base::string16& exe_path,
    base::string16* product_name,
    base::string16* version,
    base::string16* special_build,
    base::string16* channel_name) {
  *product_name = base::ASCIIToUTF16("content_shell");
  *version = base::ASCIIToUTF16(CONTENT_SHELL_VERSION);
  *special_build = base::string16();
  *channel_name = base::string16();
}
#endif

#if defined(OS_POSIX) && !defined(OS_MAC)
void ShellCrashReporterClient::GetProductNameAndVersion(
    const char** product_name,
    const char** version) {
  *product_name = "content_shell";
  *version = CONTENT_SHELL_VERSION;
}

void ShellCrashReporterClient::GetProductNameAndVersion(
    std::string* product_name,
    std::string* version,
    std::string* channel) {
  *product_name = "content_shell";
  *version = CONTENT_SHELL_VERSION;
  *channel = "";
}

base::FilePath ShellCrashReporterClient::GetReporterLogFilename() {
  return base::FilePath(FILE_PATH_LITERAL("uploads.log"));
}
#endif

#if defined(OS_WIN)
bool ShellCrashReporterClient::GetCrashDumpLocation(base::string16* crash_dir) {
#else
bool ShellCrashReporterClient::GetCrashDumpLocation(base::FilePath* crash_dir) {
#endif
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kCrashDumpsDir))
    return false;
  base::FilePath crash_directory =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          switches::kCrashDumpsDir);
#if defined(OS_WIN)
  *crash_dir = crash_directory.value();
#else
  *crash_dir = std::move(crash_directory);
#endif
  return true;
}

#if defined(OS_ANDROID)
int ShellCrashReporterClient::GetAndroidMinidumpDescriptor() {
  return kAndroidMinidumpDescriptor;
}
#endif

bool ShellCrashReporterClient::EnableBreakpadForProcess(
    const std::string& process_type) {
  return process_type == switches::kRendererProcess ||
         process_type == switches::kPpapiPluginProcess ||
         process_type == switches::kZygoteProcess ||
         process_type == switches::kGpuProcess;
}

}  // namespace content
