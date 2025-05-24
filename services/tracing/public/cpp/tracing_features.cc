// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/tracing_features.h"

#include <string>

#include "base/command_line.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/tracing_buildflags.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "components/tracing/common/tracing_switches.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"  // nogncheck
#endif

namespace features {

namespace {

BASE_FEATURE(kPerfettoBackendParams,
             "kPerfettoBackendParams",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace

// Runs the tracing service as an in-process browser service.
BASE_FEATURE(kTracingServiceInProcess,
             "TracingServiceInProcess",
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CASTOS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kEnablePerfettoSystemTracing,
             "EnablePerfettoSystemTracing",
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
             // TODO(crbug.com/42050521): Read from structured config on Fuchsia.
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kEnablePerfettoSystemBackgroundTracing,
             "EnablePerfettoSystemBackgroundTracing",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls the preferred size of each page in the shmem buffer.
BASE_FEATURE_PARAM(int,
                   kPerfettoSMBPageSizeBytes,
                   &kPerfettoBackendParams,
                   "page_size_bytes",
                   tracing::kDefaultSMBPageSizeBytes);

// Controls the size of the shared memory buffer between the current process and
// the service backend(s)
BASE_FEATURE_PARAM(int,
                   kPerfettoSharedMemorySizeBytes,
                   &kPerfettoBackendParams,
                   "shared_memory_size_bytes",
                   tracing::kDefaultSharedMemorySizeBytes);

}  // namespace features

namespace tracing {

bool ShouldSetupSystemTracing() {
#if BUILDFLAG(IS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->is_debug_android()) {
    return true;
  }
#endif  // BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::GetInstance()) {
    return base::FeatureList::IsEnabled(features::kEnablePerfettoSystemTracing);
  }
  return features::kEnablePerfettoSystemTracing.default_state ==
         base::FEATURE_ENABLED_BY_DEFAULT;
}

bool SystemBackgroundTracingEnabled() {
  return ShouldSetupSystemTracing() &&
         base::FeatureList::IsEnabled(
             features::kEnablePerfettoSystemBackgroundTracing);
}

}  // namespace tracing
