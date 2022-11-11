// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/execution_context/navigator_base.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/navigator_concurrent_hardware.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_WIN)
#include <sys/utsname.h>
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"
#endif

namespace blink {

namespace {

String GetReducedNavigatorPlatform() {
#if BUILDFLAG(IS_ANDROID)
  return "Linux armv81";
#elif BUILDFLAG(IS_MAC)
  return "MacIntel";
#elif BUILDFLAG(IS_WIN)
  return "Win32";
#elif BUILDFLAG(IS_FUCHSIA)
  return "";
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  return "Linux x86_64";
#else
#error Unsupported platform
#endif
}

}  // namespace

NavigatorBase::NavigatorBase(ExecutionContext* context)
    : NavigatorLanguage(context), ExecutionContextClient(context) {}

String NavigatorBase::userAgent() const {
  ExecutionContext* execution_context = GetExecutionContext();
  if (!execution_context)
    return String();

  execution_context->ReportNavigatorUserAgentAccess();
  return execution_context->UserAgent();
}

String NavigatorBase::platform() const {
  ExecutionContext* execution_context = GetExecutionContext();
  // Report as user agent access
  if (execution_context)
    execution_context->ReportNavigatorUserAgentAccess();

  // If the User-Agent string is opted into the SendFullUserAgentAfterReduction,
  // platform should be a full value.
  if (RuntimeEnabledFeatures::SendFullUserAgentAfterReductionEnabled(
          execution_context)) {
    return NavigatorID::platform();
  }

  // If the User-Agent string is frozen, platform should be a value
  // matching the frozen string per https://github.com/WICG/ua-client-hints.
  // See content::frozen_user_agent_strings.
  if (RuntimeEnabledFeatures::UserAgentReductionEnabled(execution_context)) {
    return GetReducedNavigatorPlatform();
  }

#if BUILDFLAG(IS_ANDROID)
  // For user-agent reduction phase 6, Android platform should be frozen
  // string, see https://www.chromium.org/updates/ua-reduction/.
  if (RuntimeEnabledFeatures::ReduceUserAgentAndroidVersionDeviceModelEnabled(
          execution_context)) {
    return GetReducedNavigatorPlatform();
  }
#else
  // For user-agent reduction phase 5, all desktop platform should be frozen
  // string, see https://www.chromium.org/updates/ua-reduction/.
  if (RuntimeEnabledFeatures::ReduceUserAgentPlatformOsCpuEnabled(
          execution_context)) {
    return GetReducedNavigatorPlatform();
  }
#endif

  return NavigatorID::platform();
}

void NavigatorBase::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  NavigatorLanguage::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  Supplementable<NavigatorBase>::Trace(visitor);
}

unsigned int NavigatorBase::hardwareConcurrency() const {
  unsigned int hardware_concurrency =
      NavigatorConcurrentHardware::hardwareConcurrency();

  probe::ApplyHardwareConcurrencyOverride(
      probe::ToCoreProbeSink(GetExecutionContext()), hardware_concurrency);
  return hardware_concurrency;
}

ExecutionContext* NavigatorBase::GetUAExecutionContext() const {
  return GetExecutionContext();
}

UserAgentMetadata NavigatorBase::GetUserAgentMetadata() const {
  ExecutionContext* execution_context = GetExecutionContext();
  return execution_context ? execution_context->GetUserAgentMetadata()
                           : blink::UserAgentMetadata();
}

}  // namespace blink
