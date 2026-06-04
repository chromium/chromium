// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tracing/ios_chrome_background_tracing_metrics_provider.h"

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "components/application_locale_storage/application_locale_storage.h"
#import "components/metrics/field_trials_provider.h"
#import "components/metrics/metrics_log.h"
#import "components/metrics/version_utils.h"
#import "components/tracing/common/background_tracing_utils.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/common/channel_info.h"
#import "third_party/metrics_proto/system_profile.pb.h"

namespace tracing {

IOSChromeBackgroundTracingMetricsProvider::
    IOSChromeBackgroundTracingMetricsProvider(
        variations::SyntheticTrialRegistry* synthetic_trial_registry)
    : synthetic_trial_registry_(synthetic_trial_registry) {}

IOSChromeBackgroundTracingMetricsProvider::
    ~IOSChromeBackgroundTracingMetricsProvider() = default;

void IOSChromeBackgroundTracingMetricsProvider::Init() {
  SetupFieldTracingFromFieldTrial();
  if (synthetic_trial_registry_) {
    system_profile_providers_.emplace_back(
        std::make_unique<variations::FieldTrialsProvider>(
            synthetic_trial_registry_, std::string_view()));
  }
}

void IOSChromeBackgroundTracingMetricsProvider::RecordCoreSystemProfileMetrics(
    metrics::SystemProfileProto& system_profile_proto) {
  if (GetApplicationContext() &&
      GetApplicationContext()->GetApplicationLocaleStorage()) {
    metrics::MetricsLog::RecordCoreSystemProfile(
        metrics::GetVersionString(), metrics::AsProtobufChannel(::GetChannel()),
        /*is_extended_stable_channel=*/false,
        GetApplicationContext()->GetApplicationLocaleStorage()->Get(),
        /*package_name=*/std::string(), &system_profile_proto);
  }
}

}  // namespace tracing
