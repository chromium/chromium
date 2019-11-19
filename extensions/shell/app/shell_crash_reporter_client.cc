// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/app/shell_crash_reporter_client.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "components/crash/core/common/crash_keys.h"
#include "components/upload_list/crash_upload_list.h"
#include "components/version_info/version_info_values.h"
#include "content/public/common/content_switches.h"
#include "extensions/shell/common/switches.h"
#include "services/service_manager/embedder/switches.h"

namespace extensions {

ShellCrashReporterClient::ShellCrashReporterClient() = default;

ShellCrashReporterClient::~ShellCrashReporterClient() = default;

void ShellCrashReporterClient::SetCrashReporterClientIdFromGUID(
    const std::string& client_guid) {
  crash_keys::SetMetricsClientIdFromGUID(client_guid);
}

void ShellCrashReporterClient::GetProductNameAndVersion(
    const char** product_name,
    const char** version) {
  DCHECK(product_name);
  DCHECK(version);
  *product_name = "AppShell_Linux";
  *version = PRODUCT_VERSION;
}

base::FilePath ShellCrashReporterClient::GetReporterLogFilename() {
  return base::FilePath(CrashUploadList::kReporterLogFilename);
}

bool ShellCrashReporterClient::GetCrashDumpLocation(base::FilePath* crash_dir) {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kCrashDumpsDir)) {
    return false;
  }

  base::FilePath crash_directory =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          switches::kCrashDumpsDir);
  if (crash_directory.empty())
    return false;

  if (!base::PathExists(crash_directory) &&
      !base::CreateDirectory(crash_directory)) {
    return false;
  }

  *crash_dir = std::move(crash_directory);
  return true;
}

bool ShellCrashReporterClient::IsRunningUnattended() {
  return false;
}

bool ShellCrashReporterClient::GetCollectStatsConsent() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableReporting);
#else
  return false;
#endif
}

bool ShellCrashReporterClient::EnableBreakpadForProcess(
    const std::string& process_type) {
  return process_type == ::switches::kRendererProcess ||
         process_type == ::switches::kPpapiPluginProcess ||
         process_type == service_manager::switches::kZygoteProcess ||
         process_type == ::switches::kGpuProcess ||
         process_type == ::switches::kUtilityProcess;
}

}  // namespace extensions
