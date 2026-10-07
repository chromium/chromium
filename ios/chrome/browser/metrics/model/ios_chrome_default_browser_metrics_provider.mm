// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/ios_chrome_default_browser_metrics_provider.h"

#import <algorithm>
#import <string_view>
#import <vector>

#import "base/check.h"
#import "base/containers/span.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/not_fatal_until.h"
#import "base/notreached.h"
#import "base/strings/strcat.h"
#import "components/metrics/metrics_log_uploader.h"
#import "components/segmentation_platform/embedder/default_model/device_switcher_model.h"
#import "components/segmentation_platform/embedder/default_model/device_switcher_result_dispatcher.h"
#import "components/segmentation_platform/public/result.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/segmentation_platform/model/segmentation_platform_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"

namespace {

constexpr std::string_view kSegmentationPlatformSignalName =
    "SegmentationPlatform";
constexpr std::string_view kAndroidSwitcherStatus = "AndroidSwitcher";
constexpr std::string_view kNotAndroidSwitcherStatus = "NotAndroidSwitcher";
constexpr std::string_view kUnknownSignalStatus = "Unknown";

// Represents a segmentation signal and its resolved status for metric slicing.
struct SegmentationSignal {
  std::string_view name;
  std::string_view status;
};

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

// Returns the device switcher signal status across loaded profiles.
std::string_view GetDeviceSwitcherSignalStatus() {
  if (experimental_flags::GetSegmentForForcedDeviceSwitcherExperience() ==
      segmentation_platform::DeviceSwitcherModel::kAndroidPhoneLabel) {
    return kAndroidSwitcherStatus;
  }

  ApplicationContext* application_context = GetApplicationContext();
  if (!application_context || !application_context->GetProfileManager()) {
    return kUnknownSignalStatus;
  }

  bool has_not_android_switcher = false;
  for (ProfileIOS* profile :
       application_context->GetProfileManager()->GetLoadedProfiles()) {
    segmentation_platform::DeviceSwitcherResultDispatcher* profile_dispatcher =
        segmentation_platform::SegmentationPlatformServiceFactory::
            GetDispatcherForProfile(profile);
    if (!profile_dispatcher) {
      continue;
    }
    segmentation_platform::ClassificationResult result =
        profile_dispatcher->GetCachedClassificationResult();
    if (!IsDeviceSwitcherClassificationReady(result)) {
      continue;
    }
    if (IsAndroidSwitcher(result)) {
      return kAndroidSwitcherStatus;
    }
    has_not_android_switcher = true;
  }

  return has_not_android_switcher ? kNotAndroidSwitcherStatus
                                  : kUnknownSignalStatus;
}

// Returns the list of segmentation signals and their statuses to record
// alongside default browser metrics.
std::vector<SegmentationSignal> GetSegmentationSignals() {
  return {{kSegmentationPlatformSignalName, GetDeviceSwitcherSignalStatus()}};
}

// Records `is_default_browser` to `histogram_name` and each signal variant
// `{histogram_name}.{signal.name}.{signal.status}`.
void RecordDefaultBrowserHistogram(
    std::string_view histogram_name,
    bool is_default_browser,
    base::span<const SegmentationSignal> signals) {
  base::UmaHistogramBoolean(histogram_name, is_default_browser);
  for (const SegmentationSignal& signal : signals) {
    base::UmaHistogramBoolean(
        base::StrCat({histogram_name, ".", signal.name, ".", signal.status}),
        is_default_browser);
  }
}

void ProvideUmaHistograms() {
  const std::vector<SegmentationSignal> signals = GetSegmentationSignals();

  RecordDefaultBrowserHistogram("IOS.IsDefaultBrowser",
                                IsChromeLikelyDefaultBrowser7Days(), signals);
  RecordDefaultBrowserHistogram("IOS.IsDefaultBrowser21",
                                IsChromeLikelyDefaultBrowser(), signals);

  RecordDefaultBrowserHistogram("IOS.IsDefaultBrowser1",
                                IsChromeLikelyDefaultBrowserXDays(1), signals);
  RecordDefaultBrowserHistogram("IOS.IsDefaultBrowser3",
                                IsChromeLikelyDefaultBrowserXDays(3), signals);
  RecordDefaultBrowserHistogram("IOS.IsDefaultBrowser14",
                                IsChromeLikelyDefaultBrowserXDays(14), signals);
  RecordDefaultBrowserHistogram("IOS.IsDefaultBrowser28",
                                IsChromeLikelyDefaultBrowserXDays(28), signals);
  RecordDefaultBrowserHistogram("IOS.IsDefaultBrowser35",
                                IsChromeLikelyDefaultBrowserXDays(35), signals);
  RecordDefaultBrowserHistogram("IOS.IsDefaultBrowser42",
                                IsChromeLikelyDefaultBrowserXDays(42), signals);

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
