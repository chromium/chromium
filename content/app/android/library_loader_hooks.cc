// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/app/android/library_loader_hooks.h"

#include "base/i18n/icu_util.h"
#include "base/logging.h"
#include "base/process/current_process.h"
#include "base/trace_event/trace_event.h"
#include "content/common/content_constants_internal.h"
#include "content/common/url_schemes.h"
#include "content/public/browser/browser_thread.h"
#include "services/tracing/public/cpp/trace_startup.h"

namespace content {

bool LibraryLoaded(JNIEnv* env,
                   jclass clazz,
                   base::android::LibraryProcessType library_process_type) {
  // Android's main browser loop is custom so we set the browser name here as
  // early as possible if this is the browser process or main webview process.
  if (library_process_type ==
          base::android::LibraryProcessType::PROCESS_BROWSER ||
      library_process_type ==
          base::android::LibraryProcessType::PROCESS_WEBVIEW) {
    base::CurrentProcess::GetInstance().SetProcessType(
        base::CurrentProcessType::PROCESS_BROWSER);
  }
  base::trace_event::TraceLog::GetInstance()->SetProcessSortIndex(
      kTraceEventBrowserProcessSortIndex);

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

#if ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_FILE
  // Initialize ICU early so that it can be used by JNI calls before
  // ContentMain() is called.
  TRACE_EVENT0("startup", "InitializeICU");
  CHECK(base::i18n::InitializeICU());
#endif

  // Content Schemes need to be registered as early as possible after the
  // CommandLine has been initialized to allow java and tests to use GURL before
  // running ContentMain.
  RegisterContentSchemes();
  return true;
}

}  // namespace content
