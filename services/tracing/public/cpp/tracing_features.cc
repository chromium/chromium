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

}  // namespace tracing
