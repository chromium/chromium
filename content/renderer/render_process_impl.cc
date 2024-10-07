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
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/system/sys_info.h"
#include "base/task/task_features.h"
#include "base/task/thread_pool/initialization_util.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "content/common/features.h"
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

namespace {

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
  size_t desired_num_threads =
      std::max(kMaxNumThreadsInForegroundPoolLowerBound,
               content::GetMinForegroundThreadsInRendererThreadPool());
  if (base::FeatureList::IsEnabled(base::kThreadPoolCap2)) {
    // Cap the threadpool to an initial fixed size.
    // Note: The size can still grow beyond the value set here
    // when tasks are blocked for a certain period of time.
    const int max_allowed_workers_per_pool =
        base::kThreadPoolCapRestrictedCount.Get();
    desired_num_threads = std::min(
        desired_num_threads, static_cast<size_t>(max_allowed_workers_per_pool));
  }
  return std::make_unique<base::ThreadPoolInstance::InitParams>(
      desired_num_threads);
}

#if BUILDFLAG(DCHECK_IS_CONFIGURABLE)
void V8DcheckCallbackHandler(const char* file, int line, const char* message) {
  // Only file/line are used from base::Location::Current() inside DCHECKs right
  // now so this should correctly pretend to be the original v8 point of
  // failure.
  ::logging::CheckError::DCheck(message,
                                base::Location::Current("", file, line));
}
#endif  // BUILDFLAG(DCHECK_IS_CONFIGURABLE)

}  // namespace

namespace content {

RenderProcessImpl::RenderProcessImpl()
    : RenderProcess(GetThreadPoolInitParams()) {
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

  SetV8FlagIfOverridden(features::kV8VmFuture, "--future", "--no-future");

  SetV8FlagIfOverridden(features::kWebAssemblyBaseline, "--liftoff",
                        "--no-liftoff");

  // V8's WASM stack switching support is sufficient to enable JavaScript
  // Promise Integration.
  SetV8FlagIfOverridden(features::kEnableExperimentalWebAssemblyJSPI,
                        "--experimental-wasm-jspi",
                        "--no-experimental-wasm-jspi");

  SetV8FlagIfOverridden(features::kWebAssemblyLazyCompilation,
                        "--wasm-lazy-compilation",
                        "--no-wasm-lazy-compilation");

  SetV8FlagIfOverridden(features::kWebAssemblyMemory64,
                        "--experimental-wasm-memory64",
                        "--no-experimental-wasm-memory64");

  SetV8FlagIfOverridden(features::kWebAssemblyTiering, "--wasm-tier-up",
                        "--no-wasm-tier-up");

  SetV8FlagIfOverridden(features::kWebAssemblyDynamicTiering,
                        "--wasm-dynamic-tiering", "--no-wasm-dynamic-tiering");

  SetV8FlagIfOverridden(blink::features::kWebAssemblyJSStringBuiltins,
                        "--experimental-wasm-imported-strings",
                        "--no-experimental-wasm-imported-strings");

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

  if (base::FeatureList::IsEnabled(features::kWebAssemblyTrapHandler)) {
    content::GetContentClient()->renderer()->SetUpWebAssemblyTrapHandler();
  }
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
  NOTREACHED_IN_MIGRATION();
}

void RenderProcessImpl::ReleaseProcess() {
  NOTREACHED_IN_MIGRATION();
}

}  // namespace content
