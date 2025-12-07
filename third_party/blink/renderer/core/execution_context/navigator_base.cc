// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/execution_context/navigator_base.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/navigator_concurrent_hardware.h"
#include "third_party/blink/renderer/core/geolocation/geolocation.h"
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
#elif BUILDFLAG(IS_IOS)
  return "iPhone";
#else
#error Unsupported platform
#endif
}

}  // namespace

NavigatorBase::NavigatorBase(ExecutionContext* context)
    : NavigatorLanguage(context), ExecutionContextClient(context) {}

String NavigatorBase::userAgent() const {
  ExecutionContext* execution_context = GetExecutionContext();
  return execution_context ? execution_context->UserAgent() : String();
}

String NavigatorBase::platform() const {
  ExecutionContext* execution_context = GetExecutionContext();

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
  visitor->Trace(global_fetch_impl_);
  ScriptWrappable::Trace(visitor);
  NavigatorLanguage::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(gpu_);
  visitor->Trace(geolocation_);
  visitor->Trace(global_fetch_impl_);
  visitor->Trace(hid_);
  visitor->Trace(lock_manager_);
  visitor->Trace(locked_mode_);
  visitor->Trace(media_capabilities_);
  visitor->Trace(navigator_ml_);
  visitor->Trace(navigator_storage_quota_);
  visitor->Trace(network_information_);
  visitor->Trace(permissions_);
  visitor->Trace(serial_);
  visitor->Trace(smart_card_resource_manager_);
  visitor->Trace(storage_bucket_manager_);
  visitor->Trace(usb_);
  visitor->Trace(wake_lock_);
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
