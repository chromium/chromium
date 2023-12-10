// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/debug/debugger.h"
#include "base/debug/stack_trace.h"
#include "base/files/file_path.h"
#include "base/i18n/icu_util.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/process/launch.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "build/build_config.h"
#include "services/service_manager/public/cpp/service_executable/service_executable_environment.h"
#include "services/service_manager/public/cpp/service_executable/service_main.h"
#include "services/service_manager/public/mojom/service.mojom.h"

#if BUILDFLAG(IS_MAC)
#include "base/apple/bundle_locations.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

namespace {

void WaitForDebuggerIfNecessary() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(::switches::kWaitForDebugger)) {
    std::vector<std::string> apps_to_debug = base::SplitString(
        command_line->GetSwitchValueASCII(::switches::kWaitForDebugger), ",",
        base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    std::string app = "launcher";
    base::FilePath exe_path =
        command_line->GetProgram().BaseName().RemoveExtension();
    for (const auto& app_name : apps_to_debug) {
      if (base::FilePath().AppendASCII(app_name) == exe_path) {
        app = app_name;
        break;
      }
    }
    if (apps_to_debug.empty() || base::Contains(apps_to_debug, app)) {
#if BUILDFLAG(IS_WIN)
      std::wstring appw = base::UTF8ToWide(app);
      std::wstring message = base::UTF8ToWide(
          base::StringPrintf("%s - %ld", app.c_str(), GetCurrentProcessId()));
      MessageBox(NULL, message.c_str(), appw.c_str(), MB_OK | MB_SETFOREGROUND);
#else
      LOG(ERROR) << app << " waiting for GDB. pid: " << getpid();
      base::debug::WaitForDebugger(60, true);
#endif
    }
  }
}

}  // namespace

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  base::CommandLine::Init(argc, argv);

#if !defined(OFFICIAL_BUILD) && BUILDFLAG(IS_WIN)
  base::RouteStdioToConsole(false);
#endif

  logging::LoggingSettings settings;
  settings.logging_dest =
      logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;
  logging::InitLogging(settings);
  // To view log output with IDs and timestamps use "adb logcat -v threadtime".
  logging::SetLogItems(true,   // Process ID
                       true,   // Thread ID
                       true,   // Timestamp
                       true);  // Tick count

  base::i18n::InitializeICU();

#if !defined(OFFICIAL_BUILD)
  // Initialize stack dumping before initializing sandbox to make sure symbol
  // names in all loaded libraries will be cached.
  base::debug::EnableInProcessStackDumping();
#endif

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  std::unique_ptr<base::FieldTrialList> field_trial_list =
      std::make_unique<base::FieldTrialList>();
  // Create field trials according to --force-fieldtrials param.
  base::FieldTrialList::CreateTrialsFromString(
      command_line->GetSwitchValueASCII(::switches::kForceFieldTrials));
  // Enable and disable features according to --enable-features and
  // --disable-features.
  std::unique_ptr<base::FeatureList> feature_list =
      std::make_unique<base::FeatureList>();
  feature_list->InitFromCommandLine(
      command_line->GetSwitchValueASCII(switches::kEnableFeatures),
      command_line->GetSwitchValueASCII(switches::kDisableFeatures));

  WaitForDebuggerIfNecessary();
  service_manager::ServiceExecutableEnvironment environment;
  ServiceMain(environment.TakeServiceRequestFromCommandLine());
  base::ThreadPoolInstance::Get()->Shutdown();

  return 0;
}
