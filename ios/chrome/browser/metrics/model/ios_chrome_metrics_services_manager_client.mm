// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/ios_chrome_metrics_services_manager_client.h"

#import <string>

#import "base/check.h"
#import "base/command_line.h"
#import "base/files/file_path.h"
#import "base/functional/bind.h"
#import "base/path_service.h"
#import "components/metrics/enabled_state_provider.h"
#import "components/metrics/metrics_state_manager.h"
#import "components/prefs/pref_service.h"
#import "components/variations/service/variations_service.h"
#import "ios/chrome/browser/metrics/model/ios_chrome_metrics_service_accessor.h"
#import "ios/chrome/browser/metrics/model/ios_chrome_metrics_service_client.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/incognito_session_tracker.h"
#import "ios/chrome/browser/shared/model/paths/paths.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/variations/model/ios_chrome_variations_service_client.h"
#import "ios/chrome/browser/variations/model/ios_ui_string_overrider_factory.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

class IOSChromeMetricsServicesManagerClient::IOSChromeEnabledStateProvider
    : public metrics::EnabledStateProvider {
 public:
  IOSChromeEnabledStateProvider() {}

  IOSChromeEnabledStateProvider(const IOSChromeEnabledStateProvider&) = delete;
  IOSChromeEnabledStateProvider& operator=(
      const IOSChromeEnabledStateProvider&) = delete;

  ~IOSChromeEnabledStateProvider() override {}

  bool IsConsentGiven() const override {
    return IOSChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled();
  }
};

IOSChromeMetricsServicesManagerClient::IOSChromeMetricsServicesManagerClient(
    PrefService* local_state)
    : enabled_state_provider_(
          std::make_unique<IOSChromeEnabledStateProvider>()),
      local_state_(local_state) {
  DCHECK(local_state);
}

IOSChromeMetricsServicesManagerClient::
    ~IOSChromeMetricsServicesManagerClient() = default;

std::unique_ptr<variations::VariationsService>
IOSChromeMetricsServicesManagerClient::CreateVariationsService(
    variations::SyntheticTrialRegistry* synthetic_trial_registry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // NOTE: On iOS, disabling background networking is not supported, so pass in
  // a dummy value for the name of the switch that disables background
  // networking.
  return variations::VariationsService::Create(
      std::make_unique<IOSChromeVariationsServiceClient>(), local_state_,
      GetMetricsStateManager(), "dummy-disable-background-switch",
      ::CreateUIStringOverrider(),
      base::BindOnce(&ApplicationContext::GetNetworkConnectionTracker,
                     base::Unretained(GetApplicationContext())),
      synthetic_trial_registry);
}

std::unique_ptr<metrics::MetricsServiceClient>
IOSChromeMetricsServicesManagerClient::CreateMetricsServiceClient(
    variations::SyntheticTrialRegistry* synthetic_trial_registry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return IOSChromeMetricsServiceClient::Create(GetMetricsStateManager(),
                                               synthetic_trial_registry);
}

metrics::MetricsStateManager*
IOSChromeMetricsServicesManagerClient::GetMetricsStateManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!metrics_state_manager_) {
    base::FilePath user_data_dir;
    base::PathService::Get(ios::DIR_USER_DATA, &user_data_dir);
    metrics_state_manager_ = metrics::MetricsStateManager::Create(
        local_state_, enabled_state_provider_.get(), std::wstring(),
        user_data_dir, metrics::StartupVisibility::kUnknown);
  }
  return metrics_state_manager_.get();
}

scoped_refptr<network::SharedURLLoaderFactory>
IOSChromeMetricsServicesManagerClient::GetURLLoaderFactory() {
  return GetApplicationContext()->GetSharedURLLoaderFactory();
}

const metrics::EnabledStateProvider&
IOSChromeMetricsServicesManagerClient::GetEnabledStateProvider() {
  return *enabled_state_provider_;
}

bool IOSChromeMetricsServicesManagerClient::IsOffTheRecordSessionActive() {
  return AreIncognitoTabsPresent();
}

// static
bool IOSChromeMetricsServicesManagerClient::AreIncognitoTabsPresent() {
  // The IncognitoSessionTracker may be null during unit tests.
  if (IncognitoSessionTracker* tracker =
          GetApplicationContext()->GetIncognitoSessionTracker()) {
    return tracker->HasIncognitoSessionTabs();
  }

  return false;
}
