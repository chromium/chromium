// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/coordinator/first_run_screen_provider.h"

#import <algorithm>

#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "base/metrics/histogram_functions.h"
#import "base/notreached.h"
#import "base/time/time.h"
#import "base/timer/elapsed_timer.h"
#import "components/regional_capabilities/regional_capabilities_service.h"
#import "components/segmentation_platform/embedder/default_model/device_switcher_model.h"
#import "components/segmentation_platform/embedder/default_model/device_switcher_result_dispatcher.h"
#import "components/segmentation_platform/public/result.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/first_run/model/first_run_metrics.h"
#import "ios/chrome/browser/first_run/public/features.h"
#import "ios/chrome/browser/regional_capabilities/model/regional_capabilities_service_factory.h"
#import "ios/chrome/browser/screen/ui_bundled/screen_provider+protected.h"
#import "ios/chrome/browser/screen/ui_bundled/screen_type.h"
#import "ios/chrome/browser/search_engine_choice/model/search_engine_choice_util.h"
#import "ios/chrome/browser/search_engine_choice/ui/search_engine_choice_ui_util.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/segmentation_platform/model/segmentation_platform_service_factory.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/public/provider/chrome/browser/signin/choice_api.h"

namespace {

// Timeout for waiting for the device switcher classification result.
constexpr base::TimeDelta kDeviceSwitcherWaitTimeout = base::Seconds(60);

// Returns true if `result` classifies the user as an Android switcher.
// A user is considered an Android switcher if the classification succeeded,
// their primary (first) label is `kAndroidPhoneLabel`, and they were not also
// classified with `kIosPhoneChromeLabel` (matching the Bring Android Tabs
// criteria).
bool IsAndroidSwitcher(
    const segmentation_platform::ClassificationResult& result) {
  return result.status == segmentation_platform::PredictionStatus::kSucceeded &&
         !result.ordered_labels.empty() &&
         result.ordered_labels[0] ==
             segmentation_platform::DeviceSwitcherModel::kAndroidPhoneLabel &&
         !std::ranges::contains(
             result.ordered_labels,
             segmentation_platform::DeviceSwitcherModel::kIosPhoneChromeLabel);
}

// Callback invoked when the segmentation device switcher classification result
// is received or times out. Records the latency and the Android switcher
// classification result.
void OnSegmentationDeviceSwitcherResult(
    base::ElapsedTimer timer,
    const segmentation_platform::ClassificationResult& result) {
  const base::TimeDelta elapsed = timer.Elapsed();

  // Determine the Android switcher classification result from the segmentation
  // platform prediction result.
  first_run::DefaultBrowserPromoSegmentationResult segmentation_result =
      first_run::DefaultBrowserPromoSegmentationResult::kNotReady;
  bool is_success = false;
  switch (result.status) {
    case segmentation_platform::PredictionStatus::kNotReady:
      segmentation_result =
          first_run::DefaultBrowserPromoSegmentationResult::kNotReady;
      break;
    case segmentation_platform::PredictionStatus::kFailed:
      segmentation_result =
          first_run::DefaultBrowserPromoSegmentationResult::kFailed;
      break;
    case segmentation_platform::PredictionStatus::kSucceeded:
      if (result.ordered_labels.empty()) {
        segmentation_result =
            first_run::DefaultBrowserPromoSegmentationResult::kFailed;
      } else if (std::ranges::contains(
                     result.ordered_labels,
                     segmentation_platform::DeviceSwitcherModel::
                         kNotSyncedLabel)) {
        segmentation_result =
            first_run::DefaultBrowserPromoSegmentationResult::kNotReady;
      } else if (IsAndroidSwitcher(result)) {
        segmentation_result =
            first_run::DefaultBrowserPromoSegmentationResult::kAndroidSwitcher;
        is_success = true;
      } else {
        segmentation_result = first_run::DefaultBrowserPromoSegmentationResult::
            kNotAndroidSwitcher;
        is_success = true;
      }
      break;
  }

  base::UmaHistogramMediumTimes(
      is_success
          ? first_run::kDefaultBrowserPromoSegmentationLatencySuccessHistogram
          : first_run::kDefaultBrowserPromoSegmentationLatencyFailureHistogram,
      elapsed);
  base::UmaHistogramEnumeration(
      first_run::kDefaultBrowserPromoSegmentationResultHistogram,
      segmentation_result);
}

// Queries the segmentation platform device switcher classification result and
// records the result and latency metrics.
void RecordSegmentationDeviceSwitcherSignal(ProfileIOS* profile) {
  if (!experimental_flags::GetSegmentForForcedDeviceSwitcherExperience()
           .empty()) {
    return;
  }
  segmentation_platform::DeviceSwitcherResultDispatcher* dispatcher =
      segmentation_platform::SegmentationPlatformServiceFactory::
          GetDispatcherForProfile(profile);
  if (!dispatcher) {
    return;
  }
  dispatcher->WaitForClassificationResult(
      kDeviceSwitcherWaitTimeout,
      base::BindOnce(&OnSegmentationDeviceSwitcherResult,
                     base::ElapsedTimer()));
}

// Queries device switcher signals to record Default Browser promo metrics
// without modifying screen visibility.
void RecordDeviceSwitcherSignals(ProfileIOS* profile) {
  RecordSegmentationDeviceSwitcherSignal(profile);
}

// Helper function to add the Best Features, Default Browser Promo, and Address
// Bar screens when kUpdatedFirstRunSequence is disabled.
void AddDBPromoAndBestFeaturesScreens(NSMutableArray* screens) {
  using enum first_run::BestFeaturesScreenVariationType;
  first_run::BestFeaturesScreenVariationType bestFeaturesType =
      first_run::GetBestFeaturesScreenVariationType();
  switch (bestFeaturesType) {
    case kGeneralScreenAfterDBPromo:
    case kGeneralScreenWithPasswordItemAfterDBPromo:
    case kShoppingUsersWithFallbackAfterDBPromo:
    case kSignedInUsersOnlyAfterDBPromo:
      [screens addObject:@(kDefaultBrowserPromo)];
      [screens addObject:@(kBestFeatures)];
      break;
    case kGeneralScreenBeforeDBPromo:
      [screens addObject:@(kBestFeatures)];
      [screens addObject:@(kDefaultBrowserPromo)];
      break;
    case kAddressBarPromoInsteadOfBestFeaturesScreen:
      // TODO(crbug.com/402429544): Add address bar promo screen.
      [screens addObject:@(kDefaultBrowserPromo)];
      break;
    case kDisabled:
    case kBestOfApp:
      [screens addObject:@(kDefaultBrowserPromo)];
      break;
  }
}

NSArray* FirstRunScreenSequenceForProfile(ProfileIOS* profile) {
  NSMutableArray* screens = [NSMutableArray array];

  BOOL shouldDisplayChoiceScreen = ShouldDisplaySearchEngineChoiceScreen(
      *profile, /*is_first_run_entrypoint=*/true,
      /*app_started_via_external_intent=*/false);

  first_run::UpdatedFRESequenceVariationType variationType =
      shouldDisplayChoiceScreen
          ? first_run::UpdatedFRESequenceVariationType::kDisabled
          : first_run::GetUpdatedFRESequenceVariation();

  BOOL hasIdentities =
      ChromeAccountManagerServiceFactory::GetForProfile(profile)
          ->HasIdentities();

  switch (variationType) {
    case first_run::UpdatedFRESequenceVariationType::kDisabled:
      [screens addObject:@(kSignIn)];
      [screens addObject:@(kHistorySync)];
      if (shouldDisplayChoiceScreen) {
        [screens addObject:@(kChoice)];
      }
      // Only add best features screen if feature
      // kUpdatedFirstRunSequence is disabled for now.
      AddDBPromoAndBestFeaturesScreens(screens);
      break;
    case first_run::UpdatedFRESequenceVariationType::kDBPromoFirst:
      [screens addObject:@(kDefaultBrowserPromo)];
      [screens addObject:@(kSignIn)];
      [screens addObject:@(kHistorySync)];
      break;
    case first_run::UpdatedFRESequenceVariationType::kRemoveSignInSync:
      if (hasIdentities) {
        [screens addObject:@(kSignIn)];
        [screens addObject:@(kHistorySync)];
      }
      [screens addObject:@(kDefaultBrowserPromo)];
      break;
    case first_run::UpdatedFRESequenceVariationType::
        kDBPromoFirstAndRemoveSignInSync:
      [screens addObject:@(kDefaultBrowserPromo)];
      if (hasIdentities) {
        [screens addObject:@(kSignIn)];
        [screens addObject:@(kHistorySync)];
      }
      break;
  }

  if (IsBestOfAppLensInteractivePromoEnabled()) {
    [screens addObject:@(kLensInteractivePromo)];
  } else if (IsBestOfAppLensAnimatedPromoEnabled()) {
    [screens addObject:@(kLensAnimatedPromo)];
  } else if (IsBestOfAppBestFeaturesEnabled()) {
    [screens addObject:@(kBestFeatures)];
  }

  // Conditionally remove the Default Browser promo if it's skipped and there
  // is a sign-in screen in the sequence. If the sign-in screen is removed,
  // do not remove the DB screen. At least one of these two screens must be
  // shown because we need the TOS disclaimer to be displayed.
  regional_capabilities::RegionalCapabilitiesService*
      regional_capabilities_service =
          ios::RegionalCapabilitiesServiceFactory::GetForProfile(profile);
  if (first_run::IsSkipDefaultBrowserPromoInFirstRunEnabled(
          regional_capabilities_service->IsInEeaCountry()) &&
      [screens containsObject:@(kSignIn)]) {
    [screens removeObject:@(kDefaultBrowserPromo)];
  }

  if (first_run::IsQueryDeviceSwitcherSignalsInFirstRunEnabled()) {
    RecordDeviceSwitcherSignals(profile);
  }

  [screens addObject:@(kStepsCompleted)];
  return screens;
}

}  // namespace

@implementation FirstRunScreenProvider

- (instancetype)initForProfile:(ProfileIOS*)profile {
  return [super initWithScreens:FirstRunScreenSequenceForProfile(profile)];
}

@end
