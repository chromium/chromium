// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/app/shell_crash_reporter_client.h"

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/public/common/content_switches.h"
#include "content/shell/common/shell_switches.h"

#if BUILDFLAG(IS_ANDROID)
#include "content/shell/android/shell_descriptors.h"
#endif

namespace content {

namespace {

base::FilePath GetCrashDumpLocationInternal() {
  base::FilePath default_dir;
#if BUILDFLAG(IS_IOS)
  CHECK(base::PathService::Get(base::DIR_CACHE, &default_dir));
  default_dir = default_dir.Append("Crashpad");
#endif  // BUILDFLAG(IS_IOS)
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kCrashDumpsDir)) {
    return default_dir;
  }
  return base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
      switches::kCrashDumpsDir);
}

}  // namespace

ShellCrashReporterClient::ShellCrashReporterClient() {}
ShellCrashReporterClient::~ShellCrashReporterClient() {}

#if BUILDFLAG(IS_WIN)
void ShellCrashReporterClient::GetProductNameAndVersion(
    const std::wstring& exe_path,
    std::wstring* product_name,
    std::wstring* version,
    std::wstring* special_build,
    std::wstring* channel_name) {
  *product_name = L"content_shell";
  *version = base::ASCIIToWide(CONTENT_SHELL_VERSION);
  *special_build = std::wstring();
  *channel_name = std::wstring();
}
#endif

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)
base::FilePath ShellCrashReporterClient::GetReporterLogFilename() {
  return base::FilePath(FILE_PATH_LITERAL("uploads.log"));
}
#endif

#if BUILDFLAG(IS_WIN)
bool ShellCrashReporterClient::GetCrashDumpLocation(std::wstring* crash_dir) {
#else
bool ShellCrashReporterClient::GetCrashDumpLocation(base::FilePath* crash_dir) {
#endif
  base::FilePath crash_directory = GetCrashDumpLocationInternal();
  if (crash_directory.empty()) {
    return false;
  }
#if BUILDFLAG(IS_WIN)
  *crash_dir = crash_directory.value();
#else
  *crash_dir = std::move(crash_directory);
#endif
  return true;
}

void ShellCrashReporterClient::GetProductInfo(ProductInfo* product_info) {
  product_info->product_name = "content_shell";
  product_info->version = CONTENT_SHELL_VERSION;
}

bool ShellCrashReporterClient::EnableBreakpadForProcess(
    const std::string& process_type) {
  return process_type == switches::kRendererProcess ||
         process_type == switches::kZygoteProcess ||
         process_type == switches::kGpuProcess;
}

}  // namespace content
