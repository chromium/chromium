// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_browser/model/utils.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/command_line.h"
#import "base/ios/ios_util.h"
#import "base/metrics/field_trial_params.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/notreached.h"
#import "base/strings/strcat.h"
#import "base/strings/string_number_conversions.h"
#import "base/time/time.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/default_browser/model/features.h"
#import "ios/chrome/browser/default_browser/promo/public/features.h"
#import "ios/chrome/browser/picture_in_picture/public/picture_in_picture_configuration.h"
#import "ios/chrome/browser/picture_in_picture/public/picture_in_picture_constants.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/commands/picture_in_picture_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/signin_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

// Key in NSUserDefaults containing an NSDictionary used to store all the
// information.
extern NSString* const kDefaultBrowserUtilsKey;

namespace {

// The video file to use for the picture-in-picture default browser video
// instructions for the app-specific settings screen.
NSString* kAppSpecificSettingsInstructionsVideo =
    @"app_specific_settings_instructions_video";

// The video file to use for the picture-in-picture default browser video
// instructions for the default apps destination.
NSString* kDefaultAppsSettingsInstructionsVideo =
    @"default_apps_settings_instructions_video";

// Time threshold for the last URL open before no URL opens likely indicates
// Chrome is no longer the default browser.
constexpr base::TimeDelta kLatestURLOpenForDefaultBrowser = base::Days(21);

// Creates storage object from legacy keys.
NSMutableDictionary<NSString*, NSObject*>* CreateStorageObjectFromLegacyKeys() {
  NSMutableDictionary<NSString*, NSObject*>* dictionary =
      [[NSMutableDictionary alloc] init];

  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  for (NSString* key in DefaultBrowserUtilsLegacyKeysForTesting()) {
    NSObject* object = [defaults objectForKey:key];
    if (object) {
      dictionary[key] = object;
      [defaults removeObjectForKey:key];
    }
  }

  return dictionary;
}

// Helper function to get the data for `key` from the storage object.
template <typename T>
T* GetObjectFromStorageForKey(NSString* key) {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  NSDictionary<NSString*, NSObject*>* storage =
      [defaults objectForKey:kDefaultBrowserUtilsKey];

  // If the storage is missing, create it, possibly from the legacy keys.
  // This is used to support loading data written by version 109 or ealier.
  // Remove once migrating data from such old version is no longer supported.
  if (!storage) {
    storage = CreateStorageObjectFromLegacyKeys();
    [defaults setObject:storage forKey:kDefaultBrowserUtilsKey];
  }

  DCHECK(storage);
  return base::apple::ObjCCast<T>(storage[key]);
}

// Helper function to update storage with `dict`. If a key in `dict` maps
// to `NSNull` instance, it will be removed from storage.
void UpdateStorageWithDictionary(NSDictionary<NSString*, NSObject*>* dict) {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  NSMutableDictionary<NSString*, NSObject*>* storage =
      [[defaults objectForKey:kDefaultBrowserUtilsKey] mutableCopy];

  // If the storage is missing, create it, possibly from the legacy keys.
  // This is used to support loading data written by version 109 or ealier.
  // Remove once migrating data from such old version is no longer supported.
  if (!storage) {
    storage = CreateStorageObjectFromLegacyKeys();
  }
  DCHECK(storage);

  for (NSString* key in dict) {
    NSObject* object = dict[key];
    if (object == [NSNull null]) {
      [storage removeObjectForKey:key];
    } else {
      storage[key] = object;
    }
  }

  [defaults setObject:storage forKey:kDefaultBrowserUtilsKey];
}

// Returns whether an event was logged for key occuring less than `delay`
// in the past.
bool HasRecordedEventForKeyLessThanDelay(NSString* key, base::TimeDelta delay) {
  NSDate* date = GetObjectFromStorageForKey<NSDate>(key);
  if (!date) {
    return false;
  }

  const base::Time time = base::Time::FromNSDate(date);
  return base::Time::Now() - time < delay;
}

// Returns the number of time the fullscreen default browser promo has been
// displayed.
NSInteger GenericPromoInteractionCount() {
  NSNumber* number =
      GetObjectFromStorageForKey<NSNumber>(kGenericPromoInteractionCount);
  return number.integerValue;
}

// Returns the number of time the tailored default browser promo has been
// displayed.
NSInteger TailoredPromoInteractionCount() {
  NSNumber* number =
      GetObjectFromStorageForKey<NSNumber>(kTailoredPromoInteractionCount);
  return number.integerValue;
}

// Returns number of days since user last interacted with one of the promos.
int NumDaysSincePromoInteraction() {
  NSDate* timestamp = GetObjectFromStorageForKey<NSDate>(
      kLastTimeUserInteractedWithFullscreenPromo);

  if (timestamp == nil) {
    return 0;
  }

  int days = (base::Time::Now() - base::Time::FromNSDate(timestamp)).InDays();
  if (days < 0) {
    return 0;
  }

  return days;
}

// Shows the default browser Picture-in-Picture.
void showDefaultBrowserPictureInPictureInstructions(
    id<PictureInPictureCommands> handler) {
  const std::string pip_param = DefaultBrowserPictureInPictureParam();
  if (pip_param == kDefaultBrowserPictureInPictureParamDisabledDefaultApps) {
    OpenIOSDefaultBrowserSettingsPage(true);
    return;
  }

  PictureInPictureConfiguration* config =
      [[PictureInPictureConfiguration alloc] init];
  NSString* view_source =
      pip_param == kDefaultBrowserPictureInPictureParamEnabledDefaultApps
          ? kDefaultAppsSettingsInstructionsVideo
          : kAppSpecificSettingsInstructionsVideo;
  config.videoURL = [[NSBundle mainBundle] URLForResource:view_source
                                            withExtension:@"mp4"];
  config.title = l10n_util::GetNSString(
      IDS_IOS_DEFAULT_BROWSER_PICTURE_IN_PICTURE_TITLE_TEXT);
  config.primaryButtonTitle =
      l10n_util::GetNSString(IDS_IOS_DEFAULT_BROWSER_PROMO_PRIMARY_BUTTON_TEXT);
  config.feature = PictureInPictureFeature::kDefaultBrowser;
  [handler showPictureInPictureWithConfig:config];
}

}  // namespace

NSString* const kLastHTTPURLOpenTime = @"lastHTTPURLOpenTime";
NSString* const kLastTimeUserInteractedWithNonModalPromo =
    @"lastTimeUserInteractedWithNonModalPromo";
NSString* const kUserInteractedWithNonModalPromoCount =
    @"userInteractedWithNonModalPromoCount";
NSString* const kLastTimeUserInteractedWithFullscreenPromo =
    @"lastTimeUserInteractedWithFullscreenPromo";

NSString* const kUserHasInteractedWithFullscreenPromo =
    @"userHasInteractedWithFullscreenPromo";
NSString* const kUserHasInteractedWithTailoredFullscreenPromo =
    @"userHasInteractedWithTailoredFullscreenPromo";
NSString* const kUserHasInteractedWithFirstRunPromo =
    @"userHasInteractedWithFirstRunPromo";
NSString* const kDisplayedFullscreenPromoCount = @"displayedPromoCount";
NSString* const kGenericPromoInteractionCount = @"genericPromoInteractionCount";
NSString* const kTailoredPromoInteractionCount =
    @"tailoredPromoInteractionCount";
constexpr base::TimeDelta kBlueDotPromoDuration = base::Days(15);
constexpr base::TimeDelta kBlueDotPromoReoccurrancePeriod = base::Days(360);
const int kDefaultBrowserSlidingWindowDays = 180;

// Migration to FET keys.
NSString* const kNonModalPromoMigrationDone = @"kNonModalPromoMigrationDone";

void SetObjectIntoStorageForKey(NSString* key, NSObject* data) {
  UpdateStorageWithDictionary(@{key : data});
}

void LogOpenHTTPURLFromExternalURL() {
  SetObjectIntoStorageForKey(kLastHTTPURLOpenTime, [NSDate date]);
}

void LogToFETDefaultBrowserPromoShown(feature_engagement::Tracker* tracker) {
  // OTR browsers can sometimes pass a null tracker, check for that here.
  if (!tracker) {
    return;
  }
  tracker->NotifyEvent(feature_engagement::events::kDefaultBrowserPromoShown);
}

bool HasDefaultBrowserBlueDotDisplayTimestamp() {
  return !GetApplicationContext()
              ->GetLocalState()
              ->FindPreference(
                  prefs::kIosDefaultBrowserBlueDotPromoFirstDisplay)
              ->IsDefaultValue();
}

void ResetDefaultBrowserBlueDotDisplayTimestampIfNeeded() {
  BOOL has_timestamp = HasDefaultBrowserBlueDotDisplayTimestamp();

  if (!has_timestamp) {
    return;
  }

  base::Time timestamp = GetApplicationContext()->GetLocalState()->GetTime(
      prefs::kIosDefaultBrowserBlueDotPromoFirstDisplay);

  // If more than `kBlueDotPromoReoccurrancePeriod` past since previous blue
  // dot display, user should again become eligible for blue dot promo.
  if (base::Time::Now() - timestamp >= kBlueDotPromoReoccurrancePeriod) {
    GetApplicationContext()->GetLocalState()->ClearPref(
        prefs::kIosDefaultBrowserBlueDotPromoFirstDisplay);
  }
}

void RecordDefaultBrowserBlueDotFirstDisplay() {
  if (!HasDefaultBrowserBlueDotDisplayTimestamp()) {
    GetApplicationContext()->GetLocalState()->SetTime(
        prefs::kIosDefaultBrowserBlueDotPromoFirstDisplay, base::Time::Now());
  }
}

bool ShouldTriggerDefaultBrowserHighlightFeature(
    feature_engagement::Tracker* tracker) {
  if (IsChromeLikelyDefaultBrowser()) {
    return false;
  }

  ResetDefaultBrowserBlueDotDisplayTimestampIfNeeded();

  if (HasDefaultBrowserBlueDotDisplayTimestamp()) {
    base::Time timestamp = GetApplicationContext()->GetLocalState()->GetTime(
        prefs::kIosDefaultBrowserBlueDotPromoFirstDisplay);
    if (base::Time::Now() - timestamp >= kBlueDotPromoDuration) {
      return false;
    }
  }

  // We ask the appropriate FET feature if it should trigger, i.e. if we
  // should show the blue dot promo badge.
  if (tracker->ShouldTriggerHelpUI(
          feature_engagement::kIPHiOSDefaultBrowserOverflowMenuBadgeFeature)) {
    tracker->Dismissed(
        feature_engagement::kIPHiOSDefaultBrowserOverflowMenuBadgeFeature);
    return true;
  }

  return false;
}

bool HasUserInteractedWithFullscreenPromoBefore() {
    // When the total promo count is 1 it means that user has seen only the FRE
    // promo. The cooldown from FRE will be taken care of in
    // ```ComputeCooldown```. Here we only need to check the timestamp of the
    // last promo if users seen more than FRE.
    return DisplayedFullscreenPromoCount() > 1 &&
           HasRecordedEventForKeyLessThanDelay(
               kLastTimeUserInteractedWithFullscreenPromo,
               base::Days(kDefaultBrowserSlidingWindowDays));
}

bool HasUserInteractedWithTailoredFullscreenPromoBefore() {
  NSNumber* number = GetObjectFromStorageForKey<NSNumber>(
      kUserHasInteractedWithTailoredFullscreenPromo);
  return number.boolValue;
}

NSInteger UserInteractionWithNonModalPromoCount() {
  NSNumber* number = GetObjectFromStorageForKey<NSNumber>(
      kUserInteractedWithNonModalPromoCount);
  return number.integerValue;
}

NSInteger DisplayedFullscreenPromoCount() {
  NSNumber* number =
      GetObjectFromStorageForKey<NSNumber>(kDisplayedFullscreenPromoCount);
  return number.integerValue;
}

void LogFullscreenDefaultBrowserPromoDisplayed() {
  const NSInteger displayed_promo_count = DisplayedFullscreenPromoCount();
  NSDictionary<NSString*, NSObject*>* update = @{
    kDisplayedFullscreenPromoCount : @(displayed_promo_count + 1),
  };

  UpdateStorageWithDictionary(update);
}

void LogUserInteractionWithFullscreenPromo() {
  const NSInteger generic_promo_interaction_count =
      GenericPromoInteractionCount();
  NSDictionary<NSString*, NSObject*>* update = @{
    kUserHasInteractedWithFullscreenPromo : @YES,
    kLastTimeUserInteractedWithFullscreenPromo : [NSDate date],
    kGenericPromoInteractionCount : @(generic_promo_interaction_count + 1),
  };

  UpdateStorageWithDictionary(update);
}

void LogUserInteractionWithTailoredFullscreenPromo() {
  const NSInteger tailored_promo_interaction_count =
      TailoredPromoInteractionCount();
  UpdateStorageWithDictionary(@{
    kUserHasInteractedWithTailoredFullscreenPromo : @YES,
    kLastTimeUserInteractedWithFullscreenPromo : [NSDate date],
    kTailoredPromoInteractionCount : @(tailored_promo_interaction_count + 1),
  });
}

void LogUserInteractionWithNonModalPromo(
    NSInteger currentNonModalPromoInteractionsCount) {
  UpdateStorageWithDictionary(@{
    kLastTimeUserInteractedWithNonModalPromo : [NSDate date],
    kUserInteractedWithNonModalPromoCount :
        @(currentNonModalPromoInteractionsCount + 1),
  });
}

void LogUserInteractionWithFirstRunPromo() {
  const NSInteger displayed_promo_count = DisplayedFullscreenPromoCount();
  UpdateStorageWithDictionary(@{
    kUserHasInteractedWithFirstRunPromo : @YES,
    kLastTimeUserInteractedWithFullscreenPromo : [NSDate date],
    kDisplayedFullscreenPromoCount : @(displayed_promo_count + 1),
  });
}

bool IsChromeLikelyDefaultBrowserXDays(int days) {
  return HasRecordedEventForKeyLessThanDelay(kLastHTTPURLOpenTime,
                                             base::Days(days));
}

bool IsChromeLikelyDefaultBrowser() {
  return HasRecordedEventForKeyLessThanDelay(kLastHTTPURLOpenTime,
                                             kLatestURLOpenForDefaultBrowser);
}

bool IsChromeLikelyDefaultBrowser7Days() {
  return HasRecordedEventForKeyLessThanDelay(kLastHTTPURLOpenTime,
                                             base::Days(7));
}

bool IsChromePotentiallyNoLongerDefaultBrowser(int likelyDefaultInterval,
                                               int likelyNotDefaultInterval) {
  bool wasLikelyDefaultBrowser =
      IsChromeLikelyDefaultBrowserXDays(likelyDefaultInterval);
  bool isStillLikelyDefaultBrowser =
      IsChromeLikelyDefaultBrowserXDays(likelyNotDefaultInterval);
  return wasLikelyDefaultBrowser && !isStillLikelyDefaultBrowser;
}

// Visible for testing.
NSString* const kDefaultBrowserUtilsKey = @"DefaultBrowserUtils";

// Visible for testing.
const NSArray<NSString*>* DefaultBrowserUtilsLegacyKeysForTesting() {
  NSArray<NSString*>* const keysForTesting = @[
    // clang-format off
    kLastHTTPURLOpenTime,
    kLastTimeUserInteractedWithFullscreenPromo,
    kLastTimeUserInteractedWithNonModalPromo,
    kUserHasInteractedWithFullscreenPromo,
    kUserHasInteractedWithTailoredFullscreenPromo,
    kUserHasInteractedWithFirstRunPromo,
    kUserInteractedWithNonModalPromoCount,
    kDisplayedFullscreenPromoCount,
    kTailoredPromoInteractionCount,
    kGenericPromoInteractionCount,
    // clang-format on
  ];

  return keysForTesting;
}

bool IsPostRestoreDefaultBrowserEligibleUser() {
  return IsFirstSessionAfterDeviceRestore() == signin::Tribool::kTrue &&
         IsChromeLikelyDefaultBrowser();
}

void LogDefaultBrowserPromoHistogramForAction(
    DefaultPromoType type,
    IOSDefaultBrowserPromoAction action) {
  switch (type) {
    case DefaultPromoTypeAllTabs:
      base::UmaHistogramEnumeration(
          "IOS.DefaultBrowserFullscreenTailoredPromoAllTabs", action);
      break;
    case DefaultPromoTypeMadeForIOS:
      base::UmaHistogramEnumeration(
          "IOS.DefaultBrowserFullscreenTailoredPromoMadeForIOS", action);
      break;
    case DefaultPromoTypeStaySafe:
      base::UmaHistogramEnumeration(
          "IOS.DefaultBrowserFullscreenTailoredPromoStaySafe", action);
      break;
    default:
      NOTREACHED();
  }
}

const base::Feature& GetFeatureForPromoReason(
    NonModalDefaultBrowserPromoReason promo_reason) {
  switch (promo_reason) {
    case NonModalDefaultBrowserPromoReason::PromoReasonOmniboxPaste:
      return feature_engagement::
          kIPHiOSPromoNonModalUrlPasteDefaultBrowserFeature;
    case NonModalDefaultBrowserPromoReason::PromoReasonAppSwitcher:
      return feature_engagement::
          kIPHiOSPromoNonModalAppSwitcherDefaultBrowserFeature;
    case NonModalDefaultBrowserPromoReason::PromoReasonShare:
      return feature_engagement::kIPHiOSPromoNonModalShareDefaultBrowserFeature;
    case NonModalDefaultBrowserPromoReason::PromoReasonNone:
      NOTREACHED();
  }
}

const std::string GetFeatureEventNameForPromoReason(
    NonModalDefaultBrowserPromoReason promo_reason) {
  switch (promo_reason) {
    case NonModalDefaultBrowserPromoReason::PromoReasonOmniboxPaste:
      return feature_engagement::events::
          kNonModalDefaultBrowserPromoUrlPasteTrigger;
    case NonModalDefaultBrowserPromoReason::PromoReasonAppSwitcher:
      return feature_engagement::events::
          kNonModalDefaultBrowserPromoAppSwitcherTrigger;
    case NonModalDefaultBrowserPromoReason::PromoReasonShare:
      return feature_engagement::events::
          kNonModalDefaultBrowserPromoShareTrigger;
    case NonModalDefaultBrowserPromoReason::PromoReasonNone:
      NOTREACHED();
  }
}

void RecordPromoDisplayStatsToUMA() {
  base::UmaHistogramCounts1000(
      "IOS.DefaultBrowserPromo.DaysSinceLastPromoInteraction",
      NumDaysSincePromoInteraction());
  base::UmaHistogramCounts100(
      "IOS.DefaultBrowserPromo.GenericPromoDisplayCount",
      GenericPromoInteractionCount());
  base::UmaHistogramCounts100(
      "IOS.DefaultBrowserPromo.TailoredPromoDisplayCount",
      TailoredPromoInteractionCount());
}

// Migration to FET

void LogNonModalPromoMigrationDone() {
  NSDictionary<NSString*, NSObject*>* update =
      @{kNonModalPromoMigrationDone : @YES};
  UpdateStorageWithDictionary(update);
}

bool IsNonModalPromoMigrationDone() {
  NSNumber* number =
      GetObjectFromStorageForKey<NSNumber>(kNonModalPromoMigrationDone);
  return number.boolValue;
}

void RecordDefaultBrowserPromoLastAction(IOSDefaultBrowserPromoAction action) {
  GetApplicationContext()->GetLocalState()->SetInteger(
      prefs::kIosDefaultBrowserPromoLastAction, static_cast<int>(action));
}

std::optional<IOSDefaultBrowserPromoAction> DefaultBrowserPromoLastAction() {
  const PrefService::Preference* last_action =
      GetApplicationContext()->GetLocalState()->FindPreference(
          prefs::kIosDefaultBrowserPromoLastAction);
  if (last_action->IsDefaultValue()) {
    return std::nullopt;
  }
  int last_action_int = last_action->GetValue()->GetInt();
  return static_cast<IOSDefaultBrowserPromoAction>(last_action_int);
}

NSDate* LastTimeUserInteractedWithNonModalPromo() {
  return GetObjectFromStorageForKey<NSDate>(
      kLastTimeUserInteractedWithNonModalPromo);
}

void OpenIOSDefaultBrowserSettingsPage(
    bool force_default_apps_if_available,
    UIApplication* ui_application_to_use,
    id<PictureInPictureCommands> pip_handler) {
  if (pip_handler && IsDefaultBrowserPictureInPictureEnabled()) {
    showDefaultBrowserPictureInPictureInstructions(pip_handler);
    return;
  }

  NSURL* url = [NSURL URLWithString:UIApplicationOpenSettingsURLString];
  if (@available(iOS 18.3, *)) {
    if (IsDefaultAppsDestinationAvailable() &&
        (force_default_apps_if_available ||
         IsUseDefaultAppsDestinationForPromosEnabled())) {
      url = [NSURL
          URLWithString:UIApplicationOpenDefaultApplicationsSettingsURLString];
    }
  }
  if (!ui_application_to_use) {
    ui_application_to_use = [UIApplication sharedApplication];
  }
  [ui_application_to_use openURL:url options:{} completionHandler:nil];
}
