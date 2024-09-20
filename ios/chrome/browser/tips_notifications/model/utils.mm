// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tips_notifications/model/utils.h"

#import "base/time/time.h"
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
    case TipsNotificationType::kError:
      NOTREACHED();
  }
}

// Returns the default trigger TimeDelta for the given `user_type`.
base::TimeDelta DefaultTriggerDelta(TipsNotificationUserType user_type) {
  switch (user_type) {
    case TipsNotificationUserType::kUnknown:
      return base::Days(3);
    case TipsNotificationUserType::kLessEngaged:
      return base::Days(21);
    case TipsNotificationUserType::kActiveSeeker:
      return base::Days(7);
  }
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
const base::TimeDelta kTipsNotificationDefaultTriggerDelta = base::Hours(72);
const char kTipsNotificationsSentPref[] = "tips_notifications.sent_bitfield";
const char kTipsNotificationsLastSent[] = "tips_notifiations.last_sent";
const char kTipsNotificationsLastTriggered[] =
    "tips_notifiations.last_triggered";
const char kTipsNotificationsLastRequestedTime[] =
    "tips_notifications.last_requested.time";
const char kTipsNotificationsUserType[] = "tips_notifications.user_type";
const char kTipsNotificationsDismissCount[] =
    "tips_notifications.dismiss_count";

bool IsTipsNotification(UNNotificationRequest* request) {
  return [request.identifier isEqualToString:kTipsNotificationId];
}

NSDictionary* UserInfoForTipsNotificationType(TipsNotificationType type) {
  return @{
    kTipsNotificationId : @YES,
    kTipsNotificationTypeKey : @(static_cast<int>(type)),
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

UNNotificationRequest* TipsNotificationRequest(
    TipsNotificationType type,
    TipsNotificationUserType user_type) {
  return [UNNotificationRequest
      requestWithIdentifier:kTipsNotificationId
                    content:ContentForTipsNotificationType(type)
                    trigger:TipsNotificationTrigger(user_type)];
}

UNNotificationContent* ContentForTipsNotificationType(
    TipsNotificationType type) {
  UNMutableNotificationContent* content =
      [[UNMutableNotificationContent alloc] init];
  ContentIDs content_ids = ContentIDsForType(type);
  content.title = l10n_util::GetNSString(content_ids.title);
  content.body = l10n_util::GetNSString(content_ids.body);
  content.userInfo = UserInfoForTipsNotificationType(type);
  content.sound = UNNotificationSound.defaultSound;
  return content;
}

base::TimeDelta TipsNotificationTriggerDelta(
    TipsNotificationUserType user_type) {
  base::TimeDelta default_trigger = DefaultTriggerDelta(user_type);
  switch (user_type) {
    case TipsNotificationUserType::kUnknown:
      return GetFieldTrialParamByFeatureAsTimeDelta(
          kIOSTipsNotifications, kIOSTipsNotificationsUnknownTriggerTimeParam,
          default_trigger);
    case TipsNotificationUserType::kLessEngaged:
      return GetFieldTrialParamByFeatureAsTimeDelta(
          kIOSTipsNotifications,
          kIOSTipsNotificationsLessEngagedTriggerTimeParam, default_trigger);
    case TipsNotificationUserType::kActiveSeeker:
      return GetFieldTrialParamByFeatureAsTimeDelta(
          kIOSTipsNotifications,
          kIOSTipsNotificationsActiveSeekerTriggerTimeParam, default_trigger);
  }
}

UNNotificationTrigger* TipsNotificationTrigger(
    TipsNotificationUserType user_type) {
  return [UNTimeIntervalNotificationTrigger
      triggerWithTimeInterval:TipsNotificationTriggerDelta(user_type)
                                  .InSecondsF()
                      repeats:NO];
}

int TipsNotificationsEnabledBitfield() {
  return GetFieldTrialParamByFeatureAsInt(kIOSTipsNotifications,
                                          kIOSTipsNotificationsEnabledParam,
                                          kEnableAllNotifications);
}

std::vector<TipsNotificationType> TipsNotificationsTypesOrder() {
  int order_num = GetFieldTrialParamByFeatureAsInt(
      kIOSTipsNotifications, kIOSTipsNotificationsOrderParam, 1);
  switch (order_num) {
    case 1:
      // The default order.
      return {
          TipsNotificationType::kSetUpListContinuation,
          TipsNotificationType::kWhatsNew,
          TipsNotificationType::kLens,
          TipsNotificationType::kOmniboxPosition,
          TipsNotificationType::kEnhancedSafeBrowsing,
          TipsNotificationType::kDefaultBrowser,
          TipsNotificationType::kDocking,
          TipsNotificationType::kSignin,
      };

    // Reordered with Lens first.
    case 2:
      return {
          TipsNotificationType::kLens,
          TipsNotificationType::kWhatsNew,
          TipsNotificationType::kSetUpListContinuation,
          TipsNotificationType::kOmniboxPosition,
          TipsNotificationType::kEnhancedSafeBrowsing,
          TipsNotificationType::kDefaultBrowser,
          TipsNotificationType::kDocking,
          TipsNotificationType::kSignin,
      };

    // Reordered with ESB first.
    case 3:
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

    // Reordered with Lens, Omnibox position, and ESB first, and SetUpList
    // removed.
    case 4:
      return {
          TipsNotificationType::kLens,
          TipsNotificationType::kOmniboxPosition,
          TipsNotificationType::kEnhancedSafeBrowsing,
          TipsNotificationType::kWhatsNew,
          TipsNotificationType::kDefaultBrowser,
          TipsNotificationType::kDocking,
          TipsNotificationType::kSignin,
      };

    default:
      NOTREACHED();
  }
}

int TipsNotificationsDismissLimit() {
  return GetFieldTrialParamByFeatureAsInt(
      kIOSTipsNotifications, kIOSTipsNotificationsDismissLimitParam, 0);
}
