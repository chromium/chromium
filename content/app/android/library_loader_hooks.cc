// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/app/android/library_loader_hooks.h"

#include "base/i18n/icu_util.h"
#include "base/logging.h"
#include "base/logging/logging_settings.h"
#include "base/process/current_process.h"
#include "base/trace_event/trace_event.h"
#include "content/common/content_constants_internal.h"
#include "content/common/url_schemes.h"
#include "content/public/browser/browser_thread.h"
#include "services/tracing/public/cpp/trace_startup.h"

namespace content {

bool LibraryLoaded(base::android::LibraryProcessType library_process_type) {
  bool is_browser_process =
      library_process_type ==
          base::android::LibraryProcessType::PROCESS_BROWSER ||
      library_process_type ==
          base::android::LibraryProcessType::PROCESS_WEBVIEW;
  // Android's main browser loop is custom so we set the browser name here as
  // early as possible if this is the browser process or main webview process.
  if (is_browser_process) {
    base::CurrentProcess::GetInstance().SetProcessType(
        base::CurrentProcessType::PROCESS_BROWSER);
  }

  // Tracing itself can only be enabled after mojo is initialized, we do so in
  // ContentMainRunnerImpl::Initialize.

  logging::LoggingSettings settings;
  settings.logging_dest =
      logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;
  logging::InitLogging(settings);
  // To view log output with IDs and timestamps use "adb logcat -v threadtime".
  logging::SetLogItems(false,    // Process ID
                       false,    // Thread ID
                       false,    // Timestamp
                       false);   // Tick count
  if (logging::GetMinLogLevel() != 0 || logging::GetVlogVerbosity() != 0 ||
      DCHECK_IS_ON()) {
    VLOG(0) << "Chromium logging enabled: level = " << logging::GetMinLogLevel()
            << ", default verbosity = " << logging::GetVlogVerbosity();
  }

  if (is_browser_process) {
    // Initialize ICU early so that it can be used by JNI calls before
    // ContentMain() is called. Only done on the browser, as we need to wait
    // until later for the renderer process in case we are running a javaless
    // renderer.
    TRACE_EVENT0("startup", "InitializeICU");
    CHECK(base::i18n::InitializeICU());
  }
  // Content Schemes need to be registered as early as possible after the
  // CommandLine has been initialized to allow java and tests to use GURL before
  // running ContentMain.
  RegisterContentSchemes();
  return true;
}

}  // namespace content
