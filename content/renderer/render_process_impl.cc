// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_process_impl.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#include <mlang.h>
#include <objidl.h>
#endif

#include <stddef.h>

#include <algorithm>
#include <utility>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/debug/crash_logging.h"
#include "base/debug/stack_trace.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/system/sys_info.h"
#include "base/task/thread_pool/initialization_util.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "content/common/thread_pool_util.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/content_renderer_client.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_frame.h"
#include "v8/include/v8-initialization.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/win_util.h"
#endif
#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && defined(ARCH_CPU_X86_64)
#include "v8/include/v8-wasm-trap-handler-posix.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "base/system/sys_info.h"
#endif

namespace {

void SetV8FlagIfFeature(const base::Feature& feature, const char* v8_flag) {
  if (base::FeatureList::IsEnabled(feature)) {
    v8::V8::SetFlagsFromString(v8_flag, strlen(v8_flag));
  }
}

void SetV8FlagIfOverridden(const base::Feature& feature,
                           const char* enabling_flag,
                           const char* disabling_flag) {
  auto overridden_state = base::FeatureList::GetStateIfOverridden(feature);
  if (!overridden_state.has_value()) {
    return;
  }
  if (overridden_state.value()) {
    v8::V8::SetFlagsFromString(enabling_flag, strlen(enabling_flag));
  } else {
    v8::V8::SetFlagsFromString(disabling_flag, strlen(disabling_flag));
  }
}

void SetV8FlagIfHasSwitch(const char* switch_name, const char* v8_flag) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switch_name)) {
    v8::V8::SetFlagsFromString(v8_flag, strlen(v8_flag));
  }
}

std::unique_ptr<base::ThreadPoolInstance::InitParams>
GetThreadPoolInitParams() {
  constexpr size_t kMaxNumThreadsInForegroundPoolLowerBound = 3;
  return std::make_unique<base::ThreadPoolInstance::InitParams>(
      std::max(kMaxNumThreadsInForegroundPoolLowerBound,
               content::GetMinForegroundThreadsInRendererThreadPool()));
}

#if BUILDFLAG(DCHECK_IS_CONFIGURABLE)
void V8DcheckCallbackHandler(const char* file, int line, const char* message) {
  // TODO(siggi): Set a crash key or a breadcrumb so the fact that we hit a
  //     V8 DCHECK gets out in the crash report.
  ::logging::LogMessage(file, line, logging::LOGGING_DCHECK).stream()
      << message;
}
#endif  // BUILDFLAG(DCHECK_IS_CONFIGURABLE)

}  // namespace

namespace content {

RenderProcessImpl::RenderProcessImpl()
    : RenderProcess(GetThreadPoolInitParams()) {
#if BUILDFLAG(IS_MAC)
  // Specified when launching the process in
  // RendererSandboxedProcessLauncherDelegate::EnableCpuSecurityMitigations
  base::SysInfo::SetIsCpuSecurityMitigationsEnabled(true);
#endif

#if BUILDFLAG(DCHECK_IS_CONFIGURABLE)
  // Some official builds ship with DCHECKs compiled in. Failing DCHECKs then
  // are either fatal or simply log the error, based on a feature flag.
  // Make sure V8 follows suit by setting a Dcheck handler that forwards to
  // the Chrome base logging implementation.
  v8::V8::SetDcheckErrorHandler(&V8DcheckCallbackHandler);

  if (!base::FeatureList::IsEnabled(base::kDCheckIsFatalFeature)) {
    // These V8 flags default on in this build configuration. This triggers
    // additional verification and code generation, which both slows down V8,
    // and can lead to fatal CHECKs. Turn these flags down to get something
    // closer to V8s normal performance and behavior.
    constexpr char kDisabledFlags[] =
        "--noturbo_verify "
        "--noturbo_verify_allocation "
        "--nodebug_code";

    v8::V8::SetFlagsFromString(kDisabledFlags, sizeof(kDisabledFlags));
  }
#endif  // BUILDFLAG(DCHECK_IS_CONFIGURABLE)

  if (base::SysInfo::IsLowEndDevice()) {
    std::string optimize_flag("--optimize-for-size");
    v8::V8::SetFlagsFromString(optimize_flag.c_str(), optimize_flag.size());
  }

  /////////////////////////////////////////////////////////////////////////////
  // V8 flags are typically set in gin/v8_initializer.cc. Only those flags
  // should be set here that cannot be set in gin/v8_initializer.cc because
  // e.g. the flag can be set in chrome://flags.
  /////////////////////////////////////////////////////////////////////////////
  SetV8FlagIfHasSwitch(switches::kDisableJavaScriptHarmonyShipping,
                       "--noharmony-shipping");
  SetV8FlagIfHasSwitch(switches::kJavaScriptHarmony, "--harmony");
  SetV8FlagIfHasSwitch(switches::kEnableExperimentalWebAssemblyFeatures,
                       "--wasm-staging");

  SetV8FlagIfFeature(features::kJavaScriptExperimentalSharedMemory,
                     "--shared-string-table --harmony-struct");

  SetV8FlagIfOverridden(features::kJavaScriptArrayGrouping,
                        "--harmony-array-grouping",
                        "--no-harmony-array-grouping");

  SetV8FlagIfOverridden(features::kV8VmFuture, "--future", "--no-future");

  SetV8FlagIfOverridden(features::kWebAssemblyBaseline, "--liftoff",
                        "--no-liftoff");

#if defined(ARCH_CPU_X86_64) || defined(ARCH_CPU_ARM64)
  // V8's WASM stack switching support is sufficient to enable JavaScript
  // Promise Integration.
  SetV8FlagIfOverridden(features::kEnableExperimentalWebAssemblyJSPI,
                        "--experimental-wasm-stack-switching",
                        "--no-experimental-wasm-stack-switching");
#endif  // defined(ARCH_CPU_X86_64) || defined(ARCH_CPU_ARM64)

  SetV8FlagIfOverridden(features::kWebAssemblyGarbageCollection,
                        "--experimental-wasm-gc", "--no-experimental-wasm-gc");

  SetV8FlagIfOverridden(features::kWebAssemblyLazyCompilation,
                        "--wasm-lazy-compilation",
                        "--no-wasm-lazy-compilation");

  SetV8FlagIfOverridden(features::kWebAssemblyRelaxedSimd,
                        "--experimental-wasm-relaxed-simd",
                        "--no-experimental-wasm-relaxed-simd");

  SetV8FlagIfOverridden(features::kWebAssemblyStringref,
                        "--experimental-wasm-stringref",
                        "--no-experimental-wasm-stringref");

  SetV8FlagIfOverridden(features::kWebAssemblyTiering, "--wasm-tier-up",
                        "--no-wasm-tier-up");

  SetV8FlagIfOverridden(features::kWebAssemblyDynamicTiering,
                        "--wasm-dynamic-tiering", "--no-wasm-dynamic-tiering");

  constexpr char kImportAssertionsFlag[] = "--harmony-import-assertions";
  v8::V8::SetFlagsFromString(kImportAssertionsFlag,
                             sizeof(kImportAssertionsFlag));

  bool enable_shared_array_buffer_unconditionally =
      base::FeatureList::IsEnabled(features::kSharedArrayBuffer);

#if !BUILDFLAG(IS_ANDROID)
  // Bypass the SAB restriction for the Finch "kill switch".
  enable_shared_array_buffer_unconditionally =
      enable_shared_array_buffer_unconditionally ||
      base::FeatureList::IsEnabled(features::kSharedArrayBufferOnDesktop);

  // Bypass the SAB restriction when enabled by Enterprise Policy.
  if (!enable_shared_array_buffer_unconditionally &&
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSharedArrayBufferUnrestrictedAccessAllowed)) {
    enable_shared_array_buffer_unconditionally = true;
    blink::WebRuntimeFeatures::EnableSharedArrayBufferUnrestrictedAccessAllowed(
        true);
  }
#endif

  if (!enable_shared_array_buffer_unconditionally) {
    // It is still possible to enable SharedArrayBuffer per context using the
    // `SharedArrayBufferConstructorEnabledCallback`. This will be done if the
    // context is cross-origin isolated or if it opts in into the reverse origin
    // trial.
    constexpr char kSABPerContextFlag[] =
        "--enable-sharedarraybuffer-per-context";
    v8::V8::SetFlagsFromString(kSABPerContextFlag, sizeof(kSABPerContextFlag));
  }

#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && defined(ARCH_CPU_X86_64)
  if (base::FeatureList::IsEnabled(features::kWebAssemblyTrapHandler)) {
    base::CommandLine* const command_line =
        base::CommandLine::ForCurrentProcess();

    if (command_line->HasSwitch(switches::kEnableCrashpad) ||
        command_line->HasSwitch(switches::kEnableCrashReporter) ||
        command_line->HasSwitch(switches::kEnableCrashReporterForTesting)) {
      // The trap handler is set as the first chance handler for Crashpad or
      // Breakpad's signal handler.
      v8::V8::EnableWebAssemblyTrapHandler(/*use_v8_signal_handler=*/false);
    } else if (!command_line->HasSwitch(
                   switches::kDisableInProcessStackTraces)) {
      if (base::debug::SetStackDumpFirstChanceCallback(
              v8::TryHandleWebAssemblyTrapPosix)) {
        // Crashpad and Breakpad are disabled, but the in-process stack dump
        // handlers are enabled, so set the callback on the stack dump handlers.
        v8::V8::EnableWebAssemblyTrapHandler(/*use_v8_signal_handler=*/false);
      } else {
        // As the registration of the callback failed, we don't enable trap
        // handlers.
      }
    } else {
      // There is no signal handler yet, but it's okay if v8 registers one.
      v8::V8::EnableWebAssemblyTrapHandler(/*use_v8_signal_handler=*/true);
    }
  }
#endif
#if BUILDFLAG(IS_WIN) && defined(ARCH_CPU_X86_64)
  if (base::FeatureList::IsEnabled(features::kWebAssemblyTrapHandler)) {
    // On Windows we use the default trap handler provided by V8.
    bool use_v8_trap_handler = true;
    v8::V8::EnableWebAssemblyTrapHandler(use_v8_trap_handler);
  }
#endif
#if BUILDFLAG(IS_MAC) && (defined(ARCH_CPU_X86_64) || defined(ARCH_CPU_ARM64))
  if (base::FeatureList::IsEnabled(features::kWebAssemblyTrapHandler)) {
    // On macOS, Crashpad uses exception ports to handle signals in a different
    // process. As we cannot just pass a callback to this other process, we ask
    // V8 to install its own signal handler to deal with WebAssembly traps.
    bool use_v8_signal_handler = true;
    v8::V8::EnableWebAssemblyTrapHandler(use_v8_signal_handler);
  }
#endif  // BUILDFLAG(IS_MAC) && defined(ARCH_CPU_X86_64)
}

RenderProcessImpl::~RenderProcessImpl() {
#ifndef NDEBUG
  int count = blink::WebFrame::InstanceCount();
  if (count)
    DLOG(ERROR) << "WebFrame LEAKED " << count << " TIMES";
#endif

  GetShutDownEvent()->Signal();
}

std::unique_ptr<RenderProcess> RenderProcessImpl::Create() {
  return base::WrapUnique(new RenderProcessImpl());
}

void RenderProcessImpl::AddRefProcess() {
  NOTREACHED();
}

void RenderProcessImpl::ReleaseProcess() {
  NOTREACHED();
}

}  // namespace content
