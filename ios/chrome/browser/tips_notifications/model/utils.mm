// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tips_notifications/model/utils.h"

#import "base/strings/string_number_conversions.h"
#import "base/strings/string_split.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "ios/chrome/browser/push_notification/model/constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Holds the l10n string ids for tips notification content.
struct ContentIDs {
  int title;
  int body;
};

// Returns the string id of the body text for the Docking promo notification.
int DockingBodyID() {
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    return IDS_IOS_NOTIFICATIONS_TIPS_DOCKING_BODY_IPAD;
  }
  return IDS_IOS_NOTIFICATIONS_TIPS_DOCKING_BODY_IPHONE;
}

// Returns the ContentIDs for the given `type`.
ContentIDs ContentIDsForType(TipsNotificationType type) {
  switch (type) {
    case TipsNotificationType::kDefaultBrowser:
      return {IDS_IOS_NOTIFICATIONS_TIPS_DEFAULT_BROWSER_TITLE,
              IDS_IOS_NOTIFICATIONS_TIPS_DEFAULT_BROWSER_BODY};
    case TipsNotificationType::kWhatsNew:
      return {IDS_IOS_NOTIFICATIONS_TIPS_WHATS_NEW_TITLE,
              IDS_IOS_NOTIFICATIONS_TIPS_WHATS_NEW_BODY};
    case TipsNotificationType::kSignin:
      return {IDS_IOS_NOTIFICATIONS_TIPS_SIGNIN_TITLE,
              IDS_IOS_NOTIFICATIONS_TIPS_SIGNIN_BODY};
    case TipsNotificationType::kSetUpListContinuation:
      return {IDS_IOS_NOTIFICATIONS_TIPS_SETUPLIST_CONTINUATION_TITLE,
              IDS_IOS_NOTIFICATIONS_TIPS_SETUPLIST_CONTINUATION_BODY};
    case TipsNotificationType::kDocking:
      return {IDS_IOS_NOTIFICATIONS_TIPS_DOCKING_TITLE, DockingBodyID()};
    case TipsNotificationType::kOmniboxPosition:
      return {IDS_IOS_NOTIFICATIONS_TIPS_OMNIBOX_POSITION_TITLE,
              IDS_IOS_NOTIFICATIONS_TIPS_OMNIBOX_POSITION_BODY};
    case TipsNotificationType::kLens:
      return {IDS_IOS_NOTIFICATIONS_TIPS_LENS_TITLE,
              IDS_IOS_NOTIFICATIONS_TIPS_LENS_BODY};
    case TipsNotificationType::kEnhancedSafeBrowsing:
      return {IDS_IOS_NOTIFICATIONS_TIPS_ENHANCED_SAFE_BROWSING_TITLE,
              IDS_IOS_NOTIFICATIONS_TIPS_ENHANCED_SAFE_BROWSING_BODY};
    case TipsNotificationType::kCPE:
      return {IDS_IOS_NOTIFICATIONS_TIPS_CPE_TITLE,
              IDS_IOS_NOTIFICATIONS_TIPS_CPE_BODY};
    case TipsNotificationType::kLensOverlay:
    case TipsNotificationType::kIncognitoLock:
    case TipsNotificationType::kError:
      NOTREACHED();
  }
}

// Returns the default trigger TimeDelta for the given `user_type` depending
// on whether this is for a reactivation notification or not.
base::TimeDelta DefaultTriggerDelta(bool for_reactivation,
                                    TipsNotificationUserType user_type) {
  if (for_reactivation) {
    return base::Days(7);
  }
  switch (user_type) {
    case TipsNotificationUserType::kUnknown:
      return base::Days(3);
    case TipsNotificationUserType::kLessEngaged:
      return base::Days(21);
    case TipsNotificationUserType::kActiveSeeker:
      return base::Days(7);
  }
}

// Parses a field trial param as a comma separated list of integers, casts the
// integers as type T, and returns a vector with elements of type T.
template <typename T>
std::vector<T> GetFieldTrialParamByFeatureAsVector(
    const base::Feature& feature,
    const std::string& param_name,
    const std::vector<T> default_values) {
  std::string param_string =
      GetFieldTrialParamValueByFeature(feature, param_name);
  if (param_string.length() == 0) {
    return default_values;
  }

  std::vector<T> values;
  const std::vector<std::string> items = base::SplitString(
      param_string, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (std::string_view item : items) {
    int value;
    if (base::StringToInt(item, &value) && value >= 0 &&
        value <= int(T::kMaxValue)) {
      values.push_back(static_cast<T>(value));
    }
  }
  return values;
}

// A bitfield with all notification types from the enum enabled.
constexpr int kAllNotificationBits =
    (1 << (int(TipsNotificationType::kMaxValue) + 1)) - 1;
// A bitfield with all notification types from the enum enabled, except for
// kError.
constexpr int kEnableAllNotifications =
    kAllNotificationBits - (1 << int(TipsNotificationType::kError));

}  // namespace

NSString* const kTipsNotificationId = @"kTipsNotificationId";
NSString* const kTipsNotificationTypeKey = @"kTipsNotificationTypeKey";
NSString* const kReactivationKey = @"kReactivationKey";
const char kTipsNotificationsSentPref[] = "tips_notifications.sent_bitfield";
const char kTipsNotificationsLastSent[] = "tips_notifiations.last_sent";
const char kTipsNotificationsLastTriggered[] =
    "tips_notifiations.last_triggered";
const char kTipsNotificationsLastRequestedTime[] =
    "tips_notifications.last_requested.time";
const char kTipsNotificationsUserType[] = "tips_notifications.user_type";
const char kTipsNotificationsDismissCount[] =
    "tips_notifications.dismiss_count";
const char kReactivationNotificationsCanceledCount[] =
    "reactivation_notifications.canceled_count";

// User defaults key for the experimental setting to force a particular
// notification type.
NSString* const kForcedTipsNotificationType = @"ForcedTipsNotificationType";
// User defaults key for the experimental setting to force a specified
// trigger time in seconds.
NSString* const kTipsNotificationTrigger = @"TipsNotificationTrigger";

bool IsTipsNotification(UNNotificationRequest* request) {
  return [request.identifier isEqualToString:kTipsNotificationId];
}

bool IsProactiveTipsNotification(UNNotificationRequest* request) {
  return [request.content.userInfo[kReactivationKey] isEqual:@YES];
}

NSDictionary* UserInfoForTipsNotificationType(TipsNotificationType type,
                                              bool for_reactivation,
                                              std::string_view profile_name) {
  return @{
    kTipsNotificationId : @YES,
    kTipsNotificationTypeKey : @(static_cast<int>(type)),
    kReactivationKey : for_reactivation ? @YES : @NO,
    kOriginatingProfileNameKey : base::SysUTF8ToNSString(profile_name),
  };
}

std::optional<TipsNotificationType> ParseTipsNotificationType(
    UNNotificationRequest* request) {
  NSDictionary* user_info = request.content.userInfo;
  NSNumber* type = user_info[kTipsNotificationTypeKey];
  if (type == nil) {
    return std::nullopt;
  }
  return static_cast<TipsNotificationType>(type.integerValue);
}

UNNotificationContent* ContentForTipsNotificationType(
    TipsNotificationType type,
    bool for_reactivation,
    std::string_view profile_name) {
  UNMutableNotificationContent* content =
      [[UNMutableNotificationContent alloc] init];
  ContentIDs content_ids = ContentIDsForType(type);
  content.title = l10n_util::GetNSString(content_ids.title);
  content.body = l10n_util::GetNSString(content_ids.body);
  content.userInfo =
      UserInfoForTipsNotificationType(type, for_reactivation, profile_name);
  content.sound = UNNotificationSound.defaultSound;
  return content;
}

base::TimeDelta TipsNotificationTriggerDelta(
    bool for_reactivation,
    TipsNotificationUserType user_type) {
  base::TimeDelta default_trigger =
      DefaultTriggerDelta(for_reactivation, user_type);
  if (for_reactivation) {
    return GetFieldTrialParamByFeatureAsTimeDelta(
        kIOSReactivationNotifications,
        kIOSReactivationNotificationsTriggerTimeParam, default_trigger);
  }
  if (int setting = TipsNotificationTriggerExperimentalSetting()) {
    return base::Seconds(setting);
  }
  return default_trigger;
}

int TipsNotificationsEnabledBitfield() {
  return kEnableAllNotifications;
}

std::vector<TipsNotificationType> TipsNotificationsTypesOrder(
    bool for_reactivation) {
  if (for_reactivation) {
    return GetFieldTrialParamByFeatureAsVector<TipsNotificationType>(
        kIOSReactivationNotifications, kIOSReactivationNotificationsOrderParam,
        {
            TipsNotificationType::kLens,
            TipsNotificationType::kEnhancedSafeBrowsing,
            TipsNotificationType::kWhatsNew,
        });
  } else if (IsIOSExpandedTipsEnabled()) {
    return GetFieldTrialParamByFeatureAsVector<TipsNotificationType>(
        kIOSExpandedTips, kIOSExpandedTipsOrderParam,
        {
            TipsNotificationType::kEnhancedSafeBrowsing,
            TipsNotificationType::kWhatsNew,
            TipsNotificationType::kLens,
            TipsNotificationType::kOmniboxPosition,
            TipsNotificationType::kSetUpListContinuation,
            TipsNotificationType::kDefaultBrowser,
            TipsNotificationType::kDocking,
            TipsNotificationType::kSignin,
            TipsNotificationType::kCPE,
        });
  }
  return {
      TipsNotificationType::kEnhancedSafeBrowsing,
      TipsNotificationType::kWhatsNew,
      TipsNotificationType::kLens,
      TipsNotificationType::kOmniboxPosition,
      TipsNotificationType::kSetUpListContinuation,
      TipsNotificationType::kDefaultBrowser,
      TipsNotificationType::kDocking,
      TipsNotificationType::kSignin,
  };
}

NotificationType NotificationTypeForTipsNotificationType(
    TipsNotificationType type) {
  switch (type) {
    case TipsNotificationType::kDefaultBrowser:
      return NotificationType::kTipsDefaultBrowser;
    case TipsNotificationType::kWhatsNew:
      return NotificationType::kTipsWhatsNew;
    case TipsNotificationType::kSignin:
      return NotificationType::kTipsSignin;
    case TipsNotificationType::kSetUpListContinuation:
      return NotificationType::kTipsSetUpListContinuation;
    case TipsNotificationType::kDocking:
      return NotificationType::kTipsDocking;
    case TipsNotificationType::kOmniboxPosition:
      return NotificationType::kTipsOmniboxPosition;
    case TipsNotificationType::kLens:
      return NotificationType::kTipsLens;
    case TipsNotificationType::kEnhancedSafeBrowsing:
      return NotificationType::kTipsEnhancedSafeBrowsing;
    case TipsNotificationType::kLensOverlay:
      return NotificationType::kTipsLensOverlay;
    case TipsNotificationType::kCPE:
      return NotificationType::kTipsCPE;
    case TipsNotificationType::kIncognitoLock:
      return NotificationType::kTipsIncognitoLock;
    case TipsNotificationType::kError:
      NOTREACHED();
  }
}

std::optional<TipsNotificationType> ForcedTipsNotificationType() {
  int int_value = [[NSUserDefaults standardUserDefaults]
                      integerForKey:kForcedTipsNotificationType] -
                  1;
  if (int_value < 0 || int_value > int(TipsNotificationType::kMaxValue)) {
    return std::nullopt;
  }
  return static_cast<TipsNotificationType>(int_value);
}

int TipsNotificationTriggerExperimentalSetting() {
  return [[NSUserDefaults standardUserDefaults]
      integerForKey:kTipsNotificationTrigger];
}
