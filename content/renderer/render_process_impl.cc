// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_process_impl.h"

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#include <objidl.h>
#include <mlang.h>
#endif

#include <stddef.h>

#include <algorithm>
#include <utility>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/debug/crash_logging.h"
#include "base/debug/stack_trace.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/sys_info.h"
#include "base/task/task_scheduler/initialization_util.h"
#include "base/time/time.h"
#include "content/common/task_scheduler.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/content_renderer_client.h"
#include "services/service_manager/embedder/switches.h"
#include "third_party/blink/public/web/web_frame.h"
#include "v8/include/v8.h"

#if defined(OS_WIN)
#include "base/win/win_util.h"
#endif

namespace {

void SetV8FlagIfFeature(const base::Feature& feature, const char* v8_flag) {
  if (base::FeatureList::IsEnabled(feature)) {
    v8::V8::SetFlagsFromString(v8_flag, strlen(v8_flag));
  }
}

void SetV8FlagIfNotFeature(const base::Feature& feature, const char* v8_flag) {
  if (!base::FeatureList::IsEnabled(feature)) {
    v8::V8::SetFlagsFromString(v8_flag, strlen(v8_flag));
  }
}

void SetV8FlagIfHasSwitch(const char* switch_name, const char* v8_flag) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switch_name)) {
    v8::V8::SetFlagsFromString(v8_flag, strlen(v8_flag));
  }
}

std::unique_ptr<base::TaskScheduler::InitParams>
GetDefaultTaskSchedulerInitParams() {

  constexpr int kMaxNumThreadsInBackgroundPool = 1;
  constexpr int kMaxNumThreadsInBackgroundBlockingPool = 1;
  constexpr int kMaxNumThreadsInForegroundPoolLowerBound = 2;
  constexpr int kMaxNumThreadsInForegroundBlockingPool = 1;
  constexpr auto kSuggestedReclaimTime = base::TimeDelta::FromSeconds(30);

  return std::make_unique<base::TaskScheduler::InitParams>(
      base::SchedulerWorkerPoolParams(kMaxNumThreadsInBackgroundPool,
                                      kSuggestedReclaimTime),
      base::SchedulerWorkerPoolParams(kMaxNumThreadsInBackgroundBlockingPool,
                                      kSuggestedReclaimTime),
      base::SchedulerWorkerPoolParams(
          std::max(
              kMaxNumThreadsInForegroundPoolLowerBound,
              content::GetMinThreadsInRendererTaskSchedulerForegroundPool()),
          kSuggestedReclaimTime),
      base::SchedulerWorkerPoolParams(kMaxNumThreadsInForegroundBlockingPool,
                                      kSuggestedReclaimTime));
}

#if DCHECK_IS_CONFIGURABLE
void V8DcheckCallbackHandler(const char* file, int line, const char* message) {
  // TODO(siggi): Set a crash key or a breadcrumb so the fact that we hit a
  //     V8 DCHECK gets out in the crash report.
  ::logging::LogMessage(file, line, logging::LOG_DCHECK).stream() << message;
}
#endif  // DCHECK_IS_CONFIGURABLE

}  // namespace

namespace content {

RenderProcessImpl::RenderProcessImpl(
    std::unique_ptr<base::TaskScheduler::InitParams> task_scheduler_init_params)
    : RenderProcess("Renderer", std::move(task_scheduler_init_params)),
      enabled_bindings_(0) {
#if DCHECK_IS_CONFIGURABLE
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
#endif  // DCHECK_IS_CONFIGURABLE

  if (base::SysInfo::IsLowEndDevice()) {
    std::string optimize_flag("--optimize-for-size");
    v8::V8::SetFlagsFromString(optimize_flag.c_str(),
                               static_cast<int>(optimize_flag.size()));
  }

  SetV8FlagIfHasSwitch(switches::kDisableJavaScriptHarmonyShipping,
                       "--noharmony-shipping");
  SetV8FlagIfHasSwitch(switches::kJavaScriptHarmony, "--harmony");

  constexpr char kModuleFlags[] =
      "--harmony-dynamic-import --harmony-import-meta";
  v8::V8::SetFlagsFromString(kModuleFlags, sizeof(kModuleFlags));

  SetV8FlagIfFeature(features::kV8Orinoco, "--no-single-threaded-gc");
  SetV8FlagIfNotFeature(features::kV8Orinoco, "--single-threaded-gc");

  SetV8FlagIfFeature(features::kV8VmFuture, "--future");
  SetV8FlagIfNotFeature(features::kV8VmFuture, "--no-future");

  SetV8FlagIfFeature(features::kWebAssemblyBaseline,
                     "--liftoff --wasm-tier-up");
  SetV8FlagIfNotFeature(features::kWebAssemblyBaseline,
                        "--no-liftoff --no-wasm-tier-up");

  if (base::FeatureList::IsEnabled(features::kWebAssemblyThreads)) {
    constexpr char kFlags[] =
        "--harmony-sharedarraybuffer "
        "--no-wasm-disable-structured-cloning "
        "--experimental-wasm-threads";

    v8::V8::SetFlagsFromString(kFlags, sizeof(kFlags));
  } else {
    SetV8FlagIfNotFeature(features::kWebAssembly,
                          "--wasm-disable-structured-cloning");
    SetV8FlagIfFeature(features::kSharedArrayBuffer,
                       "--harmony-sharedarraybuffer");
    SetV8FlagIfNotFeature(features::kSharedArrayBuffer,
                          "--no-harmony-sharedarraybuffer");
  }

  SetV8FlagIfFeature(features::kAwaitOptimization,
                     "--harmony-await-optimization");
  SetV8FlagIfNotFeature(features::kAwaitOptimization,
                        "--no-harmony-await-optimization");

  SetV8FlagIfNotFeature(features::kWebAssemblyTrapHandler,
                        "--no-wasm-trap-handler");
#if defined(OS_LINUX) && defined(ARCH_CPU_X86_64)
  if (base::FeatureList::IsEnabled(features::kWebAssemblyTrapHandler)) {
    bool use_v8_signal_handler = false;
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    if (!command_line->HasSwitch(
            service_manager::switches::kDisableInProcessStackTraces)) {
      base::debug::SetStackDumpFirstChanceCallback(v8::V8::TryHandleSignal);
    } else if (!command_line->HasSwitch(switches::kEnableCrashReporter) &&
               !command_line->HasSwitch(
                   switches::kEnableCrashReporterForTesting)) {
      // If we are using WebAssembly trap handling but both Breakpad and
      // in-process stack traces are disabled then there will be no signal
      // handler. In this case, we fall back on V8's default handler
      // (https://crbug.com/798150).
      use_v8_signal_handler = true;
    }
    // TODO(eholk): report UMA stat for how often this succeeds
    v8::V8::EnableWebAssemblyTrapHandler(use_v8_signal_handler);
  }
#endif

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  if (command_line.HasSwitch(switches::kNoV8UntrustedCodeMitigations)) {
    const char* disable_mitigations = "--no-untrusted-code-mitigations";
    v8::V8::SetFlagsFromString(disable_mitigations,
                               strlen(disable_mitigations));
  }

  if (command_line.HasSwitch(switches::kJavaScriptFlags)) {
    std::string flags(
        command_line.GetSwitchValueASCII(switches::kJavaScriptFlags));
    v8::V8::SetFlagsFromString(flags.c_str(), static_cast<int>(flags.size()));
  }

  if (command_line.HasSwitch(switches::kDomAutomationController))
    enabled_bindings_ |= BINDINGS_POLICY_DOM_AUTOMATION;
  if (command_line.HasSwitch(switches::kStatsCollectionController))
    enabled_bindings_ |= BINDINGS_POLICY_STATS_COLLECTION;
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
  auto task_scheduler_init_params =
      content::GetContentClient()->renderer()->GetTaskSchedulerInitParams();
  if (!task_scheduler_init_params)
    task_scheduler_init_params = GetDefaultTaskSchedulerInitParams();

  return base::WrapUnique(
      new RenderProcessImpl(std::move(task_scheduler_init_params)));
}

void RenderProcessImpl::AddBindings(int bindings) {
  enabled_bindings_ |= bindings;
}

int RenderProcessImpl::GetEnabledBindings() const {
  return enabled_bindings_;
}

void RenderProcessImpl::AddRefProcess() {
  NOTREACHED();
}

void RenderProcessImpl::ReleaseProcess() {
  NOTREACHED();
}

}  // namespace content
