// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/headless_crash_reporter_client.h"

#include <utility>

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/public/common/content_switches.h"
#include "headless/public/version.h"
#include "services/service_manager/embedder/switches.h"

namespace headless {

namespace {

#if defined(OS_POSIX) && !defined(OS_MACOSX)

constexpr char kChromeHeadlessProductName[] = "Chrome_Headless";

#endif

}  // namespace

HeadlessCrashReporterClient::HeadlessCrashReporterClient() = default;
HeadlessCrashReporterClient::~HeadlessCrashReporterClient() = default;

#if defined(OS_POSIX) && !defined(OS_MACOSX)
void HeadlessCrashReporterClient::GetProductNameAndVersion(
    const char** product_name,
    const char** version) {
  *product_name = kChromeHeadlessProductName;
  *version = PRODUCT_VERSION;
}

void HeadlessCrashReporterClient::GetProductNameAndVersion(
    std::string* product_name,
    std::string* version,
    std::string* channel) {
  *product_name = kChromeHeadlessProductName;
  *version = PRODUCT_VERSION;
  *channel = "";
}

base::FilePath HeadlessCrashReporterClient::GetReporterLogFilename() {
  return base::FilePath(FILE_PATH_LITERAL("uploads.log"));
}
#endif  // defined(OS_POSIX) && !defined(OS_MACOSX)

bool HeadlessCrashReporterClient::GetCrashDumpLocation(
#if defined(OS_WIN)
    base::string16* crash_dir
#else
    base::FilePath* crash_dir
#endif
    ) {
  base::FilePath crash_directory = crash_dumps_dir_;
  if (crash_directory.empty() &&
      !base::PathService::Get(base::DIR_MODULE, &crash_directory)) {
    return false;
  }
#if defined(OS_WIN)
  *crash_dir = crash_directory.value();
#else
  *crash_dir = std::move(crash_directory);
#endif
  return true;
}

bool HeadlessCrashReporterClient::EnableBreakpadForProcess(
    const std::string& process_type) {
  return process_type == ::switches::kRendererProcess ||
         process_type == ::switches::kPpapiPluginProcess ||
         process_type == service_manager::switches::kZygoteProcess ||
         process_type == ::switches::kGpuProcess;
}

}  // namespace content
