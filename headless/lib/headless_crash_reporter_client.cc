// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/headless_crash_reporter_client.h"

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/version_info/version_info.h"
#include "content/public/common/content_switches.h"

namespace headless {

namespace {

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)

constexpr char kChromeHeadlessProductName[] = "Chrome_Headless";

#endif

}  // namespace

HeadlessCrashReporterClient::HeadlessCrashReporterClient() = default;
HeadlessCrashReporterClient::~HeadlessCrashReporterClient() = default;

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)
void HeadlessCrashReporterClient::GetProductNameAndVersion(
    std::string* product_name,
    std::string* version,
    std::string* channel) {
  *product_name = kChromeHeadlessProductName;
  *version = PRODUCT_VERSION;
  *channel = "";
}
#endif  // BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)

bool HeadlessCrashReporterClient::GetCrashDumpLocation(
#if BUILDFLAG(IS_WIN)
    std::wstring* crash_dir
#else
    base::FilePath* crash_dir
#endif
) {
  base::FilePath crash_directory = crash_dumps_dir_;
  if (crash_directory.empty() &&
      !base::PathService::Get(base::DIR_TEMP, &crash_directory) &&
      !base::PathService::Get(base::DIR_MODULE, &crash_directory)) {
    return false;
  }
  if (crash_dumps_dir_.empty()) {
    crash_directory = crash_directory.Append(FILE_PATH_LITERAL("Crashpad"));
  }
#if BUILDFLAG(IS_WIN)
  *crash_dir = crash_directory.value();
#else
  *crash_dir = std::move(crash_directory);
#endif
  return true;
}

bool HeadlessCrashReporterClient::IsRunningUnattended() {
  // CHROME_HEADLESS is not equivalent to running in headless mode. This
  // environment variable is set in non-production environments which might be
  // running with crash-dumping enabled. It is used to disable certain dialogs
  // and in this particular usage disables crash report upload, while leaving
  // dumping enabled.
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  return env->HasVar("CHROME_HEADLESS");
}

bool HeadlessCrashReporterClient::GetCollectStatsConsent() {
  // Headless has no way to configure this setting so consent is implied by
  // the crash reporter being enabled.
  return true;
}

}  // namespace content
