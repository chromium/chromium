// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/app/android/library_loader_hooks.h"

#include "base/containers/fixed_flat_set.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/i18n/icu_util.h"
#include "base/logging.h"
#include "base/process/current_process.h"
#include "base/trace_event/trace_event.h"
#include "base/version_info/android/channel_getter.h"
#include "content/common/content_constants_internal.h"
#include "content/common/url_schemes.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "services/tracing/public/cpp/trace_startup.h"

namespace {
void DumpUnseenRendererJavaUsage(char const* class_name,
                                 char const* method_name) {
  // We hope to launch a Java-less renderer - crbug.com/388840315. As a part of
  // this, we need to figure out what usages of Java exist in the renderer. This
  // only exists to collect the set of Java usages. If you see a new
  // DumpWithoutCrashing from this file, simply add the problem function to the
  // list.
  static constexpr auto kKnownRendererJavaUses =
      base::MakeFixedFlatSet<std::pair<std::string_view, std::string_view>>(
          {{"java/lang/Runtime", "getRuntime"},
           {"java/lang/Runtime", "freeMemory"},
           {"java/lang/Runtime", "totalMemory"},
           {"org/chromium/android_webview/common/origin_trial/"
            "DisableOriginTrialsSafeModeUtils",
            "isDisableOriginTrialsEnabled"},
           {"org/chromium/base/ApkAssets", "open"},
           {"org/chromium/base/BuildInfo", "getAll"},
           {"org/chromium/base/EarlyTraceEvent",
            "getBackgroundStartupTracingFlag"},
           {"org/chromium/base/JavaExceptionReporter", "installHandler"},
           {"org/chromium/base/MemoryPressureListener", "addNativeCallback"},
           {"org/chromium/base/SysUtils", "isLowEndDevice"},
           {"org/chromium/base/SysUtils", "isCurrentlyLowMemory"},
           {"org/chromium/base/ThreadUtils", "setThreadPriorityAudio"},
           {"org/chromium/base/TimezoneUtils", "getDefaultTimeZoneId"},
           {"org/chromium/base/TraceEvent", "setEnabled"},
           {"org/chromium/base/TraceEvent", "setEventNameFilteringEnabled"},
           {"org/chromium/base/version_info/VersionConstantsBridge",
            "getChannel"},
           {"org/chromium/chrome/modules/stack_unwinder/"
            "StackUnwinderModuleProvider",
            "isModuleInstalled"},
           {"org/chromium/content/app/ContentChildProcessServiceDelegate",
            "setFileDescriptorsIdsToKeys"},
           {"org/chromium/media/MediaCodecUtil",
            "isDecoderSupportedForDevice"}});
  if (!kKnownRendererJavaUses.contains({class_name, method_name})) {
    SCOPED_CRASH_KEY_STRING256("Java-less Renderer", "Java Class", class_name);
    SCOPED_CRASH_KEY_STRING64("Java-less Renderer", "Java Method", method_name);
    base::debug::DumpWithoutCrashing();
  }
}
}  // namespace

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

  // Temporary, until we collect what we believe to be an exhaustive list of
  // renderer usages of Java. Feel free to delete if this is still here in 2026.
  if (version_info::android::GetChannel() == version_info::Channel::CANARY) {
    if (base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kProcessType) == switches::kRendererProcess) {
      jni_zero::SetNativeToJavaCallback(DumpUnseenRendererJavaUsage);
    }
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
