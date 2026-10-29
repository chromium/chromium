// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/ios_chrome_default_browser_metrics_provider.h"

#import <algorithm>
#import <array>
#import <optional>
#import <string_view>
#import <vector>

#import "base/barrier_callback.h"
#import "base/check.h"
#import "base/containers/span.h"
#import "base/functional/bind.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/not_fatal_until.h"
#import "base/notreached.h"
#import "base/strings/strcat.h"
#import "components/metrics/metrics_log_uploader.h"
#import "components/segmentation_platform/embedder/default_model/device_switcher_model.h"
#import "components/segmentation_platform/embedder/default_model/device_switcher_result_dispatcher.h"
#import "components/segmentation_platform/public/result.h"
#import "components/sync/service/sync_service.h"
#import "ios/chrome/browser/default_browser/model/features.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/segmentation_platform/model/segmentation_platform_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/public/provider/chrome/browser/switcher_info/switcher_info_api.h"

namespace {

constexpr std::string_view kSegmentationPlatformSignalName =
    "SegmentationPlatform";
constexpr std::string_view kSwitcherInfoSignalName = "SwitcherInfo";

// Classification status recorded for Default Browser signal histograms.
enum class SignalStatus {
  kAndroidSwitcher,
  kNotAndroidSwitcher,
  kUnknown,
  kServiceError,
};

// Returns the histogram suffix string corresponding to `status`.
std::string_view SignalStatusToString(SignalStatus status) {
  switch (status) {
    case SignalStatus::kAndroidSwitcher:
      return "AndroidSwitcher";
    case SignalStatus::kNotAndroidSwitcher:
      return "NotAndroidSwitcher";
    case SignalStatus::kUnknown:
      return "Unknown";
    case SignalStatus::kServiceError:
      return "ServiceError";
  }
}

// Captures a base Default Browser histogram name and its boolean sample at log
// creation time.
struct DefaultBrowserHistogramSample {
  std::string_view histogram_name;
  bool is_default_browser;
};

using DefaultBrowserHistogramSamples =
    std::array<DefaultBrowserHistogramSample, 8>;

// Returns true if `result` has a valid device switcher classification (i.e.,
// the prediction succeeded and sync device info did not fail or time out with
// `kNotSyncedLabel`).
bool IsDeviceSwitcherClassificationReady(
    const segmentation_platform::ClassificationResult& result) {
  return result.status == segmentation_platform::PredictionStatus::kSucceeded &&
         !result.ordered_labels.empty() &&
         !std::ranges::contains(
             result.ordered_labels,
             segmentation_platform::DeviceSwitcherModel::kNotSyncedLabel);
}

// Returns true if `result` classifies the user as an Android switcher.
// A user is considered an Android switcher if the classification is ready,
// their primary (first) label is `kAndroidPhoneLabel`, and they were not also
// classified with `kIosPhoneChromeLabel`.
bool IsAndroidSwitcher(
    const segmentation_platform::ClassificationResult& result) {
  return IsDeviceSwitcherClassificationReady(result) &&
         result.ordered_labels[0] ==
             segmentation_platform::DeviceSwitcherModel::kAndroidPhoneLabel &&
         !std::ranges::contains(
             result.ordered_labels,
             segmentation_platform::DeviceSwitcherModel::kIosPhoneChromeLabel);
}

// Returns true if `profile` has a signed-in primary account with active sync.
bool IsPrimaryAccountSyncing(ProfileIOS* profile) {
  AuthenticationService* authentication_service =
      AuthenticationServiceFactory::GetForProfile(profile);
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);
  return authentication_service &&
         authentication_service->HasPrimaryIdentity() && sync_service &&
         sync_service->GetTransportState() ==
             syncer::SyncService::TransportState::ACTIVE;
}

// Resolves the aggregated segmentation platform signal status across profiles
// with a syncing primary account.
SignalStatus ResolveSegmentationSignalStatus(
    const std::vector<
        std::optional<segmentation_platform::ClassificationResult>>& results) {
  bool has_not_android_switcher = false;
  for (const std::optional<segmentation_platform::ClassificationResult>&
           result : results) {
    if (!result.has_value() || !IsDeviceSwitcherClassificationReady(*result)) {
      continue;
    }
    if (IsAndroidSwitcher(*result)) {
      return SignalStatus::kAndroidSwitcher;
    }
    has_not_android_switcher = true;
  }
  // `results` is only populated for profiles with a syncing primary account;
  // reaching here without any valid classification means every queried profile
  // failed to return a ready result.
  return has_not_android_switcher ? SignalStatus::kNotAndroidSwitcher
                                  : SignalStatus::kServiceError;
}

// Returns the device switcher signal status across loaded profiles.
SignalStatus GetDeviceSwitcherSignalStatus() {
  if (experimental_flags::GetSegmentForForcedDeviceSwitcherExperience() ==
      segmentation_platform::DeviceSwitcherModel::kAndroidPhoneLabel) {
    return SignalStatus::kAndroidSwitcher;
  }

  std::vector<std::optional<segmentation_platform::ClassificationResult>>
      results;
  ApplicationContext* application_context = GetApplicationContext();
  if (application_context && application_context->GetProfileManager()) {
    for (ProfileIOS* profile :
         application_context->GetProfileManager()->GetLoadedProfiles()) {
      if (!IsPrimaryAccountSyncing(profile)) {
        continue;
      }
      segmentation_platform::DeviceSwitcherResultDispatcher*
          profile_dispatcher =
              segmentation_platform::SegmentationPlatformServiceFactory::
                  GetDispatcherForProfile(profile);
      if (!profile_dispatcher) {
        results.push_back(std::nullopt);
        continue;
      }
      results.push_back(profile_dispatcher->GetCachedClassificationResult());
    }
  }

  if (results.empty()) {
    return SignalStatus::kUnknown;
  }
  return ResolveSegmentationSignalStatus(results);
}

// Resolves the aggregated switcher info signal status across profiles.
SignalStatus ResolveSwitcherInfoSignalStatus(
    const std::vector<std::optional<ios::provider::SwitcherInfoResult>>&
        results) {
  bool has_not_android_switcher = false;
  for (const std::optional<ios::provider::SwitcherInfoResult>& result :
       results) {
    if (!result.has_value()) {
      continue;
    }
    switch (result->switcher_status) {
      case ios::provider::SwitcherStatus::kSwitcher:
        return SignalStatus::kAndroidSwitcher;
      case ios::provider::SwitcherStatus::kNotSwitcher:
        has_not_android_switcher = true;
        break;
      case ios::provider::SwitcherStatus::kUnknown:
        break;
    }
  }
  return has_not_android_switcher ? SignalStatus::kNotAndroidSwitcher
                                  : SignalStatus::kServiceError;
}

// Records `{sample.histogram_name}.{signal_name}.{signal_status}` for each
// sample in `samples`.
void RecordSignalHistograms(
    std::string_view signal_name,
    SignalStatus signal_status,
    base::span<const DefaultBrowserHistogramSample> samples) {
  std::string_view status_string = SignalStatusToString(signal_status);
  for (const DefaultBrowserHistogramSample& sample : samples) {
    base::UmaHistogramBoolean(base::StrCat({sample.histogram_name, ".",
                                            signal_name, ".", status_string}),
                              sample.is_default_browser);
  }
}

// Callback invoked when all `GetSwitcherInfo` queries across loaded profiles
// have completed.
void OnSwitcherInfoResults(
    DefaultBrowserHistogramSamples samples,
    std::vector<std::optional<ios::provider::SwitcherInfoResult>> results) {
  RecordSignalHistograms(kSwitcherInfoSignalName,
                         ResolveSwitcherInfoSignalStatus(results), samples);
}

// Queries `ios::provider::GetSwitcherInfo` across signed-in loaded profiles and
// emits `.SwitcherInfo.{SignalStatus}` histograms upon completion.
void RecordSwitcherInfoSignalHistograms(
    const DefaultBrowserHistogramSamples& samples) {
  std::vector<id<SystemIdentity>> identities;
  ApplicationContext* application_context = GetApplicationContext();
  if (application_context && application_context->GetProfileManager()) {
    for (ProfileIOS* profile :
         application_context->GetProfileManager()->GetLoadedProfiles()) {
      AuthenticationService* authentication_service =
          AuthenticationServiceFactory::GetForProfile(profile);
      ChromeAccountManagerService* account_manager_service =
          ChromeAccountManagerServiceFactory::GetForProfile(profile);
      if (!authentication_service || !account_manager_service ||
          !authentication_service->HasPrimaryIdentity()) {
        continue;
      }
      for (id<SystemIdentity> identity in account_manager_service
               ->GetAllIdentities()) {
        identities.push_back(identity);
      }
    }
  }

  if (identities.empty()) {
    RecordSignalHistograms(kSwitcherInfoSignalName, SignalStatus::kUnknown,
                           samples);
    return;
  }

  base::RepeatingCallback<void(
      std::optional<ios::provider::SwitcherInfoResult>)>
      barrier_callback = base::BarrierCallback<
          std::optional<ios::provider::SwitcherInfoResult>>(
          identities.size(), base::BindOnce(&OnSwitcherInfoResults, samples));
  for (id<SystemIdentity> identity : identities) {
    ios::provider::GetSwitcherInfo(identity, barrier_callback);
  }
}

void ProvideUmaHistograms() {
  const DefaultBrowserHistogramSamples samples = {{
      {"IOS.IsDefaultBrowser", IsChromeLikelyDefaultBrowser7Days()},
      {"IOS.IsDefaultBrowser21", IsChromeLikelyDefaultBrowser()},
      {"IOS.IsDefaultBrowser1", IsChromeLikelyDefaultBrowserXDays(1)},
      {"IOS.IsDefaultBrowser3", IsChromeLikelyDefaultBrowserXDays(3)},
      {"IOS.IsDefaultBrowser14", IsChromeLikelyDefaultBrowserXDays(14)},
      {"IOS.IsDefaultBrowser28", IsChromeLikelyDefaultBrowserXDays(28)},
      {"IOS.IsDefaultBrowser35", IsChromeLikelyDefaultBrowserXDays(35)},
      {"IOS.IsDefaultBrowser42", IsChromeLikelyDefaultBrowserXDays(42)},
  }};

  for (const DefaultBrowserHistogramSample& sample : samples) {
    base::UmaHistogramBoolean(sample.histogram_name, sample.is_default_browser);
  }

  RecordSignalHistograms(kSegmentationPlatformSignalName,
                         GetDeviceSwitcherSignalStatus(), samples);
  if (IsQuerySwitcherInfoSignalForDefaultBrowserMetricsEnabled()) {
    RecordSwitcherInfoSignalHistograms(samples);
  }

  base::UmaHistogramBoolean("IOS.DefaultBrowserAbandonment21To7",
                            IsChromePotentiallyNoLongerDefaultBrowser(21, 7));
  base::UmaHistogramBoolean("IOS.DefaultBrowserAbandonment28To14",
                            IsChromePotentiallyNoLongerDefaultBrowser(28, 14));
  base::UmaHistogramBoolean("IOS.DefaultBrowserAbandonment35To14",
                            IsChromePotentiallyNoLongerDefaultBrowser(35, 14));
  base::UmaHistogramBoolean("IOS.DefaultBrowserAbandonment42To21",
                            IsChromePotentiallyNoLongerDefaultBrowser(42, 21));
}

}  // namespace

IOSChromeDefaultBrowserMetricsProvider::IOSChromeDefaultBrowserMetricsProvider(
    metrics::MetricsLogUploader::MetricServiceType metrics_service_type)
    : metrics_service_type_(metrics_service_type) {}

IOSChromeDefaultBrowserMetricsProvider::
    ~IOSChromeDefaultBrowserMetricsProvider() {}

void IOSChromeDefaultBrowserMetricsProvider::OnDidCreateMetricsLog() {
  if (metrics_service_type_ ==
      metrics::MetricsLogUploader::MetricServiceType::UMA) {
    ProvideUmaHistograms();
  }

  emitted_ = true;
}

void IOSChromeDefaultBrowserMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  switch (metrics_service_type_) {
    case metrics::MetricsLogUploader::MetricServiceType::UMA:
      if (!emitted_) {
        ProvideUmaHistograms();
      }
      return;
    case metrics::MetricsLogUploader::MetricServiceType::UKM:
      // `this` should never be instantiated with this service type.
      NOTREACHED();
    case metrics::MetricsLogUploader::MetricServiceType::STRUCTURED_METRICS:
      // `this` should never be instantiated with this service type.
      NOTREACHED();
    case metrics::MetricsLogUploader::MetricServiceType::DWA:
      // `this` should never be instantiated with this service type.
      NOTREACHED();
    case metrics::MetricsLogUploader::MetricServiceType::PRIVATE_METRICS:
      // `this` should never be instantiated with this service type.
      NOTREACHED();
  }
  NOTREACHED();
}
