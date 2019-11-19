// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/tracing_features.h"

#include <string>

#include "base/command_line.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "components/tracing/common/tracing_switches.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"  // nogncheck
#endif

namespace features {

// Causes the BackgroundTracingManager to upload proto messages via UMA,
// rather than JSON via the crash frontend.
const base::Feature kBackgroundTracingProtoOutput{
  "BackgroundTracingProtoOutput",
#if defined(OS_ANDROID)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Causes Perfetto to run in-process mode for in-process tracing producers.
const base::Feature kPerfettoForceOutOfProcessProducer{
    "PerfettoForceOutOfProcessProducer", base::FEATURE_DISABLED_BY_DEFAULT};

// Runs the tracing service as an in-process browser service.
const base::Feature kTracingServiceInProcess {
  "TracingServiceInProcess",
#if defined(OS_ANDROID) || defined(IS_CHROMECAST)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

const base::Feature kEnablePerfettoSystemTracing{
    "EnablePerfettoSystemTracing", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features

namespace tracing {

bool ShouldSetupSystemTracing() {
#if defined(OS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->is_debug_android()) {
    return true;
  }
#endif  // defined(OS_ANDROID)
  if (base::FeatureList::GetInstance()) {
    return base::FeatureList::IsEnabled(features::kEnablePerfettoSystemTracing);
  }
  return features::kEnablePerfettoSystemTracing.default_state ==
         base::FEATURE_ENABLED_BY_DEFAULT;
}

}  // namespace tracing
