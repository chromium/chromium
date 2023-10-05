// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin_promo_view_mediator.h"

#import <memory>

#import "base/feature_list.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/sync/base/features.h"
#import "components/sync/service/sync_user_settings.h"
#import "ios/chrome/browser/discover_feed/feed_constants.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_observer_bridge.h"
#import "ios/chrome/browser/signin/system_identity.h"
#import "ios/chrome/browser/sync/model/sync_observer_bridge.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_configurator.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_consumer.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_coordinator.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_utils.h"
#import "ios/chrome/browser/ui/authentication/signin_presenter.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_feature.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Number of times the sign-in promo should be displayed until it is
// automatically dismissed.
constexpr int kAutomaticSigninPromoViewDismissCount = 20;
// User defaults key to get the last logged impression of the top-of-feed promo.
NSString* const kLastSigninImpressionTopOfFeedKey =
    @"last_signin_impression_top_of_feed";
// The time representing a session to increment the impression count of the
// top-of-feed promo, in seconds.
constexpr int kTopOfFeedSessionTimeInterval = 60 * 30;

// Returns true if the sign-in promo is supported for `access_point`.
bool IsSupportedAccessPoint(signin_metrics::AccessPoint access_point) {
  switch (access_point) {
    case signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_MANAGER:
    case signin_metrics::AccessPoint::ACCESS_POINT_RECENT_TABS:
    case signin_metrics::AccessPoint::ACCESS_POINT_NTP_FEED_TOP_PROMO:
    case signin_metrics::AccessPoint::ACCESS_POINT_READING_LIST:
      return true;
    case signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS:
    // TODO(crbug.com/1478824): Pass ACCESS_POINT_TAB_SWITCHER and not recent
    // tabs in the tab switcher promo.
    case signin_metrics::AccessPoint::ACCESS_POINT_TAB_SWITCHER:
    case signin_metrics::AccessPoint::
        ACCESS_POINT_POST_DEVICE_RESTORE_SIGNIN_PROMO:
    case signin_metrics::AccessPoint::ACCESS_POINT_NTP_FEED_CARD_MENU_PROMO:
    case signin_metrics::AccessPoint::ACCESS_POINT_NTP_FEED_BOTTOM_PROMO:
    case signin_metrics::AccessPoint::ACCESS_POINT_START_PAGE:
    case signin_metrics::AccessPoint::ACCESS_POINT_NTP_LINK:
    case signin_metrics::AccessPoint::ACCESS_POINT_MENU:
    case signin_metrics::AccessPoint::ACCESS_POINT_SUPERVISED_USER:
    case signin_metrics::AccessPoint::ACCESS_POINT_EXTENSION_INSTALL_BUBBLE:
    case signin_metrics::AccessPoint::ACCESS_POINT_EXTENSIONS:
    case signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_BUBBLE:
    case signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN:
    case signin_metrics::AccessPoint::ACCESS_POINT_USER_MANAGER:
    case signin_metrics::AccessPoint::ACCESS_POINT_DEVICES_PAGE:
    case signin_metrics::AccessPoint::ACCESS_POINT_CLOUD_PRINT:
    case signin_metrics::AccessPoint::ACCESS_POINT_SIGNIN_PROMO:
    case signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN:
    case signin_metrics::AccessPoint::ACCESS_POINT_PASSWORD_BUBBLE:
    case signin_metrics::AccessPoint::ACCESS_POINT_AUTOFILL_DROPDOWN:
    case signin_metrics::AccessPoint::ACCESS_POINT_NTP_CONTENT_SUGGESTIONS:
    case signin_metrics::AccessPoint::ACCESS_POINT_RESIGNIN_INFOBAR:
    case signin_metrics::AccessPoint::ACCESS_POINT_MACHINE_LOGON:
    case signin_metrics::AccessPoint::ACCESS_POINT_GOOGLE_SERVICES_SETTINGS:
    case signin_metrics::AccessPoint::ACCESS_POINT_SYNC_ERROR_CARD:
    case signin_metrics::AccessPoint::ACCESS_POINT_FORCED_SIGNIN:
    case signin_metrics::AccessPoint::ACCESS_POINT_ACCOUNT_RENAMED:
    case signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN:
    case signin_metrics::AccessPoint::ACCESS_POINT_SAFETY_CHECK:
    case signin_metrics::AccessPoint::ACCESS_POINT_KALEIDOSCOPE:
    case signin_metrics::AccessPoint::
        ACCESS_POINT_ENTERPRISE_SIGNOUT_COORDINATOR:
    case signin_metrics::AccessPoint::
        ACCESS_POINT_SIGNIN_INTERCEPT_FIRST_RUN_EXPERIENCE:
    case signin_metrics::AccessPoint::ACCESS_POINT_SEND_TAB_TO_SELF_PROMO:
    case signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS_SYNC_OFF_ROW:
    case signin_metrics::AccessPoint::
        ACCESS_POINT_POST_DEVICE_RESTORE_BACKGROUND_SIGNIN:
    case signin_metrics::AccessPoint::ACCESS_POINT_NTP_SIGNED_OUT_ICON:
    case signin_metrics::AccessPoint::ACCESS_POINT_DESKTOP_SIGNIN_MANAGER:
    case signin_metrics::AccessPoint::ACCESS_POINT_FOR_YOU_FRE:
    case signin_metrics::AccessPoint::ACCESS_POINT_CREATOR_FEED_FOLLOW:
    case signin_metrics::AccessPoint::ACCESS_POINT_REAUTH_INFO_BAR:
    case signin_metrics::AccessPoint::ACCESS_POINT_ACCOUNT_CONSISTENCY_SERVICE:
    case signin_metrics::AccessPoint::ACCESS_POINT_SEARCH_COMPANION:
    case signin_metrics::AccessPoint::ACCESS_POINT_SET_UP_LIST:
    case signin_metrics::AccessPoint::
        ACCESS_POINT_PASSWORD_MIGRATION_WARNING_ANDROID:
    case signin_metrics::AccessPoint::ACCESS_POINT_SAVE_TO_PHOTOS_IOS:
    case signin_metrics::AccessPoint::ACCESS_POINT_MAX:
      return false;
  }
}

// Records in histogram, the number of times the sign-in promo is displayed
// before the sign-in button is pressed.
void RecordImpressionsTilSigninButtonsHistogramForAccessPoint(
    signin_metrics::AccessPoint access_point,
    int displayed_count) {
  switch (access_point) {
    case signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_MANAGER:
      base::UmaHistogramCounts100(
          "MobileSignInPromo.BookmarkManager.ImpressionsTilSigninButtons",
          displayed_count);
      break;
    case signin_metrics::AccessPoint::ACCESS_POINT_NTP_FEED_TOP_PROMO:
      base::UmaHistogramCounts100(
          "MobileSignInPromo.NTPFeedTop.ImpressionsTilSigninButtons",
          displayed_count);
      break;
    case signin_metrics::AccessPoint::ACCESS_POINT_READING_LIST:
      base::UmaHistogramCounts100(
          "MobileSignInPromo.ReadingList.ImpressionsTilSigninButtons",
          displayed_count);
      break;
    case signin_metrics::AccessPoint::
        ACCESS_POINT_ENTERPRISE_SIGNOUT_COORDINATOR:
    case signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS:
    case signin_metrics::AccessPoint::ACCESS_POINT_RECENT_TABS:
    case signin_metrics::AccessPoint::ACCESS_POINT_TAB_SWITCHER:
    case signin_metrics::AccessPoint::ACCESS_POINT_START_PAGE:
    case signin_metrics::AccessPoint::ACCESS_POINT_NTP_LINK:
    case signin_metrics::AccessPoint::ACCESS_POINT_MENU:
    case signin_metrics::AccessPoint::ACCESS_POINT_SUPERVISED_USER:
    case signin_metrics::AccessPoint::ACCESS_POINT_EXTENSION_INSTALL_BUBBLE:
    case signin_metrics::AccessPoint::ACCESS_POINT_EXTENSIONS:
    case signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_BUBBLE:
    case signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN:
    case signin_metrics::AccessPoint::ACCESS_POINT_USER_MANAGER:
    case signin_metrics::AccessPoint::ACCESS_POINT_DEVICES_PAGE:
    case signin_metrics::AccessPoint::ACCESS_POINT_CLOUD_PRINT:
    case signin_metrics::AccessPoint::ACCESS_POINT_SIGNIN_PROMO:
    case signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN:
    case signin_metrics::AccessPoint::ACCESS_POINT_PASSWORD_BUBBLE:
    case signin_metrics::AccessPoint::ACCESS_POINT_AUTOFILL_DROPDOWN:
    case signin_metrics::AccessPoint::ACCESS_POINT_NTP_CONTENT_SUGGESTIONS:
    case signin_metrics::AccessPoint::ACCESS_POINT_RESIGNIN_INFOBAR:
    case signin_metrics::AccessPoint::ACCESS_POINT_MACHINE_LOGON:
    case signin_metrics::AccessPoint::ACCESS_POINT_GOOGLE_SERVICES_SETTINGS:
    case signin_metrics::AccessPoint::ACCESS_POINT_SYNC_ERROR_CARD:
    case signin_metrics::AccessPoint::ACCESS_POINT_FORCED_SIGNIN:
    case signin_metrics::AccessPoint::ACCESS_POINT_ACCOUNT_RENAMED:
    case signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN:
    case signin_metrics::AccessPoint::ACCESS_POINT_SAFETY_CHECK:
    case signin_metrics::AccessPoint::ACCESS_POINT_KALEIDOSCOPE:
    case signin_metrics::AccessPoint::
        ACCESS_POINT_SIGNIN_INTERCEPT_FIRST_RUN_EXPERIENCE:
    case signin_metrics::AccessPoint::ACCESS_POINT_SEND_TAB_TO_SELF_PROMO:
    case signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS_SYNC_OFF_ROW:
    case signin_metrics::AccessPoint::
        ACCESS_POINT_POST_DEVICE_RESTORE_SIGNIN_PROMO:
    case signin_metrics::AccessPoint::
        ACCESS_POINT_POST_DEVICE_RESTORE_BACKGROUND_SIGNIN:
    case signin_metrics::AccessPoint::ACCESS_POINT_NTP_SIGNED_OUT_ICON:
    case signin_metrics::AccessPoint::ACCESS_POINT_NTP_FEED_CARD_MENU_PROMO:
    case signin_metrics::AccessPoint::ACCESS_POINT_NTP_FEED_BOTTOM_PROMO:
    case signin_metrics::AccessPoint::ACCESS_POINT_DESKTOP_SIGNIN_MANAGER:
    case signin_metrics::AccessPoint::ACCESS_POINT_FOR_YOU_FRE:
    case signin_metrics::AccessPoint::ACCESS_POINT_CREATOR_FEED_FOLLOW:
    case signin_metrics::AccessPoint::ACCESS_POINT_REAUTH_INFO_BAR:
    case signin_metrics::AccessPoint::ACCESS_POINT_ACCOUNT_CONSISTENCY_SERVICE:
    case signin_metrics::AccessPoint::ACCESS_POINT_SEARCH_COMPANION:
    case signin_metrics::AccessPoint::ACCESS_POINT_SET_UP_LIST:
    case signin_metrics::AccessPoint::
        ACCESS_POINT_PASSWORD_MIGRATION_WARNING_ANDROID:
    case signin_metrics::AccessPoint::ACCESS_POINT_SAVE_TO_PHOTOS_IOS:
    case signin_metrics::AccessPoint::ACCESS_POINT_MAX:
      NOTREACHED() << "Unexpected value for access point "
                   << static_cast<int>(access_point);
      break;
  }
}

// Records in histogram, the number of times the sign-in promo is displayed
// before the cancel button is pressed.
void RecordImpressionsTilDismissHistogramForAccessPoint(
    signin_metrics::AccessPoint access_point,
    int displayed_count) {
  switch (access_point) {
    case signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_MANAGER:
      base::UmaHistogramCounts100(
          "MobileSignInPromo.BookmarkManager.ImpressionsTilDismiss",
          displayed_count);
      break;
    case signin_metrics::AccessPoint::ACCESS_POINT_NTP_FEED_TOP_PROMO:
      base::UmaHistogramCounts100(
          "MobileSignInPromo.NTPFeedTop.ImpressionsTilDismiss",
          displayed_count);
      break;
    case signin_metrics::AccessPoint::ACCESS_POINT_READING_LIST:
      base::UmaHistogramCounts100(
          "MobileSignInPromo.ReadingList.ImpressionsTilDismiss",
          displayed_count);
      break;
    case signin_metrics::AccessPoint::
        ACCESS_POINT_ENTERPRISE_SIGNOUT_COORDINATOR:
    case signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS:
    case signin_metrics::AccessPoint::ACCESS_POINT_RECENT_TABS:
    case signin_metrics::AccessPoint::ACCESS_POINT_TAB_SWITCHER:
    case signin_metrics::AccessPoint::ACCESS_POINT_START_PAGE:
    case signin_metrics::AccessPoint::ACCESS_POINT_NTP_LINK:
    case signin_metrics::AccessPoint::ACCESS_POINT_MENU:
    case signin_metrics::AccessPoint::ACCESS_POINT_SUPERVISED_USER:
    case signin_metrics::AccessPoint::ACCESS_POINT_EXTENSION_INSTALL_BUBBLE:
    case signin_metrics::AccessPoint::ACCESS_POINT_EXTENSIONS:
    case signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_BUBBLE:
    case signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN:
    case signin_metrics::AccessPoint::ACCESS_POINT_USER_MANAGER:
    case signin_metrics::AccessPoint::ACCESS_POINT_DEVICES_PAGE:
    case signin_metrics::AccessPoint::ACCESS_POINT_CLOUD_PRINT:
    case signin_metrics::AccessPoint::ACCESS_POINT_SIGNIN_PROMO:
    case signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN:
    case signin_metrics::AccessPoint::ACCESS_POINT_PASSWORD_BUBBLE:
    case signin_metrics::AccessPoint::ACCESS_POINT_AUTOFILL_DROPDOWN:
    case signin_metrics::AccessPoint::ACCESS_POINT_NTP_CONTENT_SUGGESTIONS:
    case signin_metrics::AccessPoint::ACCESS_POINT_RESIGNIN_INFOBAR:
    case signin_metrics::AccessPoint::ACCESS_POINT_MACHINE_LOGON:
    case signin_metrics::AccessPoint::ACCESS_POINT_GOOGLE_SERVICES_SETTINGS:
    case signin_metrics::AccessPoint::ACCESS_POINT_SYNC_ERROR_CARD:
    case signin_metrics::AccessPoint::ACCESS_POINT_FORCED_SIGNIN:
    case signin_metrics::AccessPoint::ACCESS_POINT_ACCOUNT_RENAMED:
    case signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN:
    case signin_metrics::AccessPoint::ACCESS_POINT_SAFETY_CHECK:
    case signin_metrics::AccessPoint::ACCESS_POINT_KALEIDOSCOPE:
    case signin_metrics::AccessPoint::
        ACCESS_POINT_SIGNIN_INTERCEPT_FIRST_RUN_EXPERIENCE:
    case signin_metrics::AccessPoint::ACCESS_POINT_SEND_TAB_TO_SELF_PROMO:
    case signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS_SYNC_OFF_ROW:
    case signin_metrics::AccessPoint::
        ACCESS_POINT_POST_DEVICE_RESTORE_SIGNIN_PROMO:
    case signin_metrics::AccessPoint::
        ACCESS_POINT_POST_DEVICE_RESTORE_BACKGROUND_SIGNIN:
    case signin_metrics::AccessPoint::ACCESS_POINT_NTP_SIGNED_OUT_ICON:
    case signin_metrics::AccessPoint::ACCESS_POINT_NTP_FEED_CARD_MENU_PROMO:
    case signin_metrics::AccessPoint::ACCESS_POINT_NTP_FEED_BOTTOM_PROMO:
    case signin_metrics::AccessPoint::ACCESS_POINT_DESKTOP_SIGNIN_MANAGER:
    case signin_metrics::AccessPoint::ACCESS_POINT_FOR_YOU_FRE:
    case signin_metrics::AccessPoint::ACCESS_POINT_CREATOR_FEED_FOLLOW:
    case signin_metrics::AccessPoint::ACCESS_POINT_REAUTH_INFO_BAR:
    case signin_metrics::AccessPoint::ACCESS_POINT_ACCOUNT_CONSISTENCY_SERVICE:
    case signin_metrics::AccessPoint::ACCESS_POINT_SEARCH_COMPANION:
    case signin_metrics::AccessPoint::ACCESS_POINT_SET_UP_LIST:
    case signin_metrics::AccessPoint::
        ACCESS_POINT_PASSWORD_MIGRATION_WARNING_ANDROID:
    case signin_metrics::AccessPoint::ACCESS_POINT_SAVE_TO_PHOTOS_IOS:
    case signin_metrics::AccessPoint::ACCESS_POINT_MAX:
      NOTREACHED() << "Unexpected value for access point "
                   << static_cast<int>(access_point);
      break;
  }
}

// Records in histogram, the number of times the sign-in promo is displayed
// before the close button is pressed.
void RecordImpressionsTilXButtonHistogramForAccessPoint(
    signin_metrics::AccessPoint access_point,
    int displayed_count) {
  switch (access_point) {
    case signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_MANAGER:
      base::UmaHistogramCounts100(
          "MobileSignInPromo.BookmarkManager.ImpressionsTilXButton",
          displayed_count);
      break;
    case signin_metrics::AccessPoint::ACCESS_POINT_NTP_FEED_TOP_PROMO:
      base::UmaHistogramCounts100(
          "MobileSignInPromo.NTPFeedTop.ImpressionsTilXButton",
          displayed_count);
      break;
    case signin_metrics::AccessPoint::ACCESS_POINT_READING_LIST:
      base::UmaHistogramCounts100(
          "MobileSignInPromo.ReadingList.ImpressionsTilXButton",
          displayed_count);
      break;
    case signin_metrics::AccessPoint::
        ACCESS_POINT_ENTERPRISE_SIGNOUT_COORDINATOR:
    case signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS:
    case signin_metrics::AccessPoint::ACCESS_POINT_RECENT_TABS:
    case signin_metrics::AccessPoint::ACCESS_POINT_TAB_SWITCHER:
    case signin_metrics::AccessPoint::ACCESS_POINT_START_PAGE:
    case signin_metrics::AccessPoint::ACCESS_POINT_NTP_LINK:
    case signin_metrics::AccessPoint::ACCESS_POINT_MENU:
    case signin_metrics::AccessPoint::ACCESS_POINT_SUPERVISED_USER:
    case signin_metrics::AccessPoint::ACCESS_POINT_EXTENSION_INSTALL_BUBBLE:
    case signin_metrics::AccessPoint::ACCESS_POINT_EXTENSIONS:
    case signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_BUBBLE:
    case signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN:
    case signin_metrics::AccessPoint::ACCESS_POINT_USER_MANAGER:
    case signin_metrics::AccessPoint::ACCESS_POINT_DEVICES_PAGE:
    case signin_metrics::AccessPoint::ACCESS_POINT_CLOUD_PRINT:
    case signin_metrics::AccessPoint::ACCESS_POINT_SIGNIN_PROMO:
    case signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN:
    case signin_metrics::AccessPoint::ACCESS_POINT_PASSWORD_BUBBLE:
    case signin_metrics::AccessPoint::ACCESS_POINT_AUTOFILL_DROPDOWN:
    case signin_metrics::AccessPoint::ACCESS_POINT_NTP_CONTENT_SUGGESTIONS:
    case signin_metrics::AccessPoint::ACCESS_POINT_RESIGNIN_INFOBAR:
    case signin_metrics::AccessPoint::ACCESS_POINT_MACHINE_LOGON:
    case signin_metrics::AccessPoint::ACCESS_POINT_GOOGLE_SERVICES_SETTINGS:
    case signin_metrics::AccessPoint::ACCESS_POINT_SYNC_ERROR_CARD:
    case signin_metrics::AccessPoint::ACCESS_POINT_FORCED_SIGNIN:
    case signin_metrics::AccessPoint::ACCESS_POINT_ACCOUNT_RENAMED:
    case signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN:
    case signin_metrics::AccessPoint::ACCESS_POINT_SAFETY_CHECK:
    case signin_metrics::AccessPoint::ACCESS_POINT_KALEIDOSCOPE:
    case signin_metrics::AccessPoint::
        ACCESS_POINT_SIGNIN_INTERCEPT_FIRST_RUN_EXPERIENCE:
    case signin_metrics::AccessPoint::ACCESS_POINT_SEND_TAB_TO_SELF_PROMO:
    case signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS_SYNC_OFF_ROW:
    case signin_metrics::AccessPoint::
        ACCESS_POINT_POST_DEVICE_RESTORE_SIGNIN_PROMO:
    case signin_metrics::AccessPoint::
        ACCESS_POINT_POST_DEVICE_RESTORE_BACKGROUND_SIGNIN:
    case signin_metrics::AccessPoint::ACCESS_POINT_NTP_SIGNED_OUT_ICON:
    case signin_metrics::AccessPoint::ACCESS_POINT_NTP_FEED_CARD_MENU_PROMO:
    case signin_metrics::AccessPoint::ACCESS_POINT_NTP_FEED_BOTTOM_PROMO:
    case signin_metrics::AccessPoint::ACCESS_POINT_DESKTOP_SIGNIN_MANAGER:
    case signin_metrics::AccessPoint::ACCESS_POINT_FOR_YOU_FRE:
    case signin_metrics::AccessPoint::ACCESS_POINT_CREATOR_FEED_FOLLOW:
    case signin_metrics::AccessPoint::ACCESS_POINT_REAUTH_INFO_BAR:
    case signin_metrics::AccessPoint::ACCESS_POINT_ACCOUNT_CONSISTENCY_SERVICE:
    case signin_metrics::AccessPoint::ACCESS_POINT_SEARCH_COMPANION:
    case signin_metrics::AccessPoint::ACCESS_POINT_SET_UP_LIST:
    case signin_metrics::AccessPoint::
        ACCESS_POINT_PASSWORD_MIGRATION_WARNING_ANDROID:
    case signin_metrics::AccessPoint::ACCESS_POINT_SAVE_TO_PHOTOS_IOS:
    case signin_metrics::AccessPoint::ACCESS_POINT_MAX:
      NOTREACHED() << "Unexpected value for access point "
                   << static_cast<int>(access_point);
      break;
  }
}

// Returns the DisplayedCount preference key string for `access_point`.
const char* DisplayedCountPreferenceKey(
    signin_metrics::AccessPoint access_point) {
  switch (access_point) {
    case signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_MANAGER:
      return prefs::kIosBookmarkSigninPromoDisplayedCount;
    case signin_metrics::AccessPoint::ACCESS_POINT_NTP_FEED_TOP_PROMO:
      return prefs::kIosNtpFeedTopSigninPromoDisplayedCount;
    case signin_metrics::AccessPoint::ACCESS_POINT_READING_LIST:
      return prefs::kIosReadingListSigninPromoDisplayedCount;
    case signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS:
    case signin_metrics::AccessPoint::ACCESS_POINT_RECENT_TABS:
    case signin_metrics::AccessPoint::ACCESS_POINT_TAB_SWITCHER:
    case signin_metrics::AccessPoint::ACCESS_POINT_START_PAGE:
    case signin_metrics::AccessPoint::ACCESS_POINT_NTP_LINK:
    case signin_metrics::AccessPoint::ACCESS_POINT_MENU:
    case signin_metrics::AccessPoint::ACCESS_POINT_SUPERVISED_USER:
    case signin_metrics::AccessPoint::ACCESS_POINT_EXTENSION_INSTALL_BUBBLE:
    case signin_metrics::AccessPoint::ACCESS_POINT_EXTENSIONS:
    case signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_BUBBLE:
    case signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN:
    case signin_metrics::AccessPoint::ACCESS_POINT_USER_MANAGER:
    case signin_metrics::AccessPoint::ACCESS_POINT_DEVICES_PAGE:
    case signin_metrics::AccessPoint::ACCESS_POINT_CLOUD_PRINT:
    case signin_metrics::AccessPoint::ACCESS_POINT_SIGNIN_PROMO:
    case signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN:
    case signin_metrics::AccessPoint::ACCESS_POINT_PASSWORD_BUBBLE:
    case signin_metrics::AccessPoint::ACCESS_POINT_AUTOFILL_DROPDOWN:
    case signin_metrics::AccessPoint::ACCESS_POINT_NTP_CONTENT_SUGGESTIONS:
    case signin_metrics::AccessPoint::ACCESS_POINT_RESIGNIN_INFOBAR:
    case signin_metrics::AccessPoint::ACCESS_POINT_MACHINE_LOGON:
    case signin_metrics::AccessPoint::ACCESS_POINT_GOOGLE_SERVICES_SETTINGS:
    case signin_metrics::AccessPoint::ACCESS_POINT_SYNC_ERROR_CARD:
    case signin_metrics::AccessPoint::ACCESS_POINT_FORCED_SIGNIN:
    case signin_metrics::AccessPoint::ACCESS_POINT_ACCOUNT_RENAMED:
    case signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN:
    case signin_metrics::AccessPoint::ACCESS_POINT_SAFETY_CHECK:
    case signin_metrics::AccessPoint::ACCESS_POINT_KALEIDOSCOPE:
    case signin_metrics::AccessPoint::
        ACCESS_POINT_ENTERPRISE_SIGNOUT_COORDINATOR:
    case signin_metrics::AccessPoint::
        ACCESS_POINT_SIGNIN_INTERCEPT_FIRST_RUN_EXPERIENCE:
    case signin_metrics::AccessPoint::ACCESS_POINT_SEND_TAB_TO_SELF_PROMO:
    case signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS_SYNC_OFF_ROW:
    case signin_metrics::AccessPoint::
        ACCESS_POINT_POST_DEVICE_RESTORE_SIGNIN_PROMO:
    case signin_metrics::AccessPoint::
        ACCESS_POINT_POST_DEVICE_RESTORE_BACKGROUND_SIGNIN:
    case signin_metrics::AccessPoint::ACCESS_POINT_NTP_SIGNED_OUT_ICON:
    case signin_metrics::AccessPoint::ACCESS_POINT_NTP_FEED_CARD_MENU_PROMO:
    case signin_metrics::AccessPoint::ACCESS_POINT_NTP_FEED_BOTTOM_PROMO:
    case signin_metrics::AccessPoint::ACCESS_POINT_DESKTOP_SIGNIN_MANAGER:
    case signin_metrics::AccessPoint::ACCESS_POINT_FOR_YOU_FRE:
    case signin_metrics::AccessPoint::ACCESS_POINT_CREATOR_FEED_FOLLOW:
    case signin_metrics::AccessPoint::ACCESS_POINT_REAUTH_INFO_BAR:
    case signin_metrics::AccessPoint::ACCESS_POINT_ACCOUNT_CONSISTENCY_SERVICE:
    case signin_metrics::AccessPoint::ACCESS_POINT_SEARCH_COMPANION:
    case signin_metrics::AccessPoint::ACCESS_POINT_SET_UP_LIST:
    case signin_metrics::AccessPoint::
        ACCESS_POINT_PASSWORD_MIGRATION_WARNING_ANDROID:
    case signin_metrics::AccessPoint::ACCESS_POINT_SAVE_TO_PHOTOS_IOS:
    case signin_metrics::AccessPoint::ACCESS_POINT_MAX:
      return nullptr;
  }
}

// Returns AlreadySeen preference key string for `access_point`.
const char* AlreadySeenSigninViewPreferenceKey(
    signin_metrics::AccessPoint access_point) {
  switch (access_point) {
    case signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_MANAGER:
      return prefs::kIosBookmarkPromoAlreadySeen;
    case signin_metrics::AccessPoint::ACCESS_POINT_NTP_FEED_TOP_PROMO:
      return prefs::kIosNtpFeedTopPromoAlreadySeen;
    case signin_metrics::AccessPoint::ACCESS_POINT_READING_LIST:
      return prefs::kIosReadingListPromoAlreadySeen;
    case signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS:
    case signin_metrics::AccessPoint::ACCESS_POINT_RECENT_TABS:
    case signin_metrics::AccessPoint::ACCESS_POINT_TAB_SWITCHER:
    case signin_metrics::AccessPoint::ACCESS_POINT_START_PAGE:
    case signin_metrics::AccessPoint::ACCESS_POINT_NTP_LINK:
    case signin_metrics::AccessPoint::ACCESS_POINT_MENU:
    case signin_metrics::AccessPoint::ACCESS_POINT_SUPERVISED_USER:
    case signin_metrics::AccessPoint::ACCESS_POINT_EXTENSION_INSTALL_BUBBLE:
    case signin_metrics::AccessPoint::ACCESS_POINT_EXTENSIONS:
    case signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_BUBBLE:
    case signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN:
    case signin_metrics::AccessPoint::ACCESS_POINT_USER_MANAGER:
    case signin_metrics::AccessPoint::ACCESS_POINT_DEVICES_PAGE:
    case signin_metrics::AccessPoint::ACCESS_POINT_CLOUD_PRINT:
    case signin_metrics::AccessPoint::ACCESS_POINT_SIGNIN_PROMO:
    case signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN:
    case signin_metrics::AccessPoint::ACCESS_POINT_PASSWORD_BUBBLE:
    case signin_metrics::AccessPoint::ACCESS_POINT_AUTOFILL_DROPDOWN:
    case signin_metrics::AccessPoint::ACCESS_POINT_NTP_CONTENT_SUGGESTIONS:
    case signin_metrics::AccessPoint::ACCESS_POINT_RESIGNIN_INFOBAR:
    case signin_metrics::AccessPoint::ACCESS_POINT_MACHINE_LOGON:
    case signin_metrics::AccessPoint::ACCESS_POINT_GOOGLE_SERVICES_SETTINGS:
    case signin_metrics::AccessPoint::ACCESS_POINT_SYNC_ERROR_CARD:
    case signin_metrics::AccessPoint::ACCESS_POINT_FORCED_SIGNIN:
    case signin_metrics::AccessPoint::ACCESS_POINT_ACCOUNT_RENAMED:
    case signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN:
    case signin_metrics::AccessPoint::ACCESS_POINT_SAFETY_CHECK:
    case signin_metrics::AccessPoint::ACCESS_POINT_KALEIDOSCOPE:
    case signin_metrics::AccessPoint::
        ACCESS_POINT_ENTERPRISE_SIGNOUT_COORDINATOR:
    case signin_metrics::AccessPoint::
        ACCESS_POINT_SIGNIN_INTERCEPT_FIRST_RUN_EXPERIENCE:
    case signin_metrics::AccessPoint::ACCESS_POINT_SEND_TAB_TO_SELF_PROMO:
    case signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS_SYNC_OFF_ROW:
    case signin_metrics::AccessPoint::
        ACCESS_POINT_POST_DEVICE_RESTORE_SIGNIN_PROMO:
    case signin_metrics::AccessPoint::
        ACCESS_POINT_POST_DEVICE_RESTORE_BACKGROUND_SIGNIN:
    case signin_metrics::AccessPoint::ACCESS_POINT_NTP_SIGNED_OUT_ICON:
    case signin_metrics::AccessPoint::ACCESS_POINT_NTP_FEED_CARD_MENU_PROMO:
    case signin_metrics::AccessPoint::ACCESS_POINT_NTP_FEED_BOTTOM_PROMO:
    case signin_metrics::AccessPoint::ACCESS_POINT_DESKTOP_SIGNIN_MANAGER:
    case signin_metrics::AccessPoint::ACCESS_POINT_FOR_YOU_FRE:
    case signin_metrics::AccessPoint::ACCESS_POINT_CREATOR_FEED_FOLLOW:
    case signin_metrics::AccessPoint::ACCESS_POINT_REAUTH_INFO_BAR:
    case signin_metrics::AccessPoint::ACCESS_POINT_ACCOUNT_CONSISTENCY_SERVICE:
    case signin_metrics::AccessPoint::ACCESS_POINT_SEARCH_COMPANION:
    case signin_metrics::AccessPoint::ACCESS_POINT_SET_UP_LIST:
    case signin_metrics::AccessPoint::
        ACCESS_POINT_PASSWORD_MIGRATION_WARNING_ANDROID:
    case signin_metrics::AccessPoint::ACCESS_POINT_SAVE_TO_PHOTOS_IOS:
    case signin_metrics::AccessPoint::ACCESS_POINT_MAX:
      return nullptr;
  }
}

// See documentation of displayedIdentity property.
id<SystemIdentity> GetDisplayedIdentity(
    AuthenticationService* authService,
    ChromeAccountManagerService* accountManagerService) {
  if (authService->HasPrimaryIdentity(signin::ConsentLevel::kSignin)) {
    return authService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  }
  DCHECK(accountManagerService);
  return accountManagerService->GetDefaultIdentity();
}

}  // namespace

@interface SigninPromoViewMediator () <ChromeAccountManagerServiceObserver,
                                       SyncObserverModelBridge>

// Redefined to be readwrite. See documentation in the header file.
@property(nonatomic, strong, readwrite) id<SystemIdentity> displayedIdentity;

// YES if the sign-in flow is in progress.
@property(nonatomic, assign, readwrite) BOOL signinInProgress;
// YES if the initial sync for a specific data type is in progress. The data
// type is based on `dataTypeToWaitForInitialSync`.
@property(nonatomic, assign, readwrite) BOOL initialSyncInProgress;

// Presenter which can show signin UI.
@property(nonatomic, weak, readonly) id<SigninPresenter> presenter;

// User's preferences service.
@property(nonatomic, assign) PrefService* prefService;

// AccountManager Service used to retrive identities.
@property(nonatomic, assign) ChromeAccountManagerService* accountManagerService;

// Authentication Service for the user's signed-in state.
@property(nonatomic, assign) AuthenticationService* authService;

// The access point for the sign-in promo view.
@property(nonatomic, assign, readonly) signin_metrics::AccessPoint accessPoint;

// The avatar of `displayedIdentity`.
@property(nonatomic, strong) UIImage* displayedIdentityAvatar;

// YES if the sign-in promo is currently visible by the user.
@property(nonatomic, assign, getter=isSigninPromoViewVisible)
    BOOL signinPromoViewVisible;

// YES if the sign-in promo is either invalid or closed.
@property(nonatomic, assign, readonly, getter=isInvalidOrClosed)
    BOOL invalidOrClosed;

@end

@implementation SigninPromoViewMediator {
  std::unique_ptr<ChromeAccountManagerServiceObserverBridge>
      _accountManagerServiceObserver;
  // View used to present sign-in UI.
  UIViewController* _baseViewController;
  // Sync service.
  syncer::SyncService* _syncService;
  // Observer for changes to the sync state.
  std::unique_ptr<SyncObserverBridge> _syncObserverBridge;
}

+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry {
  // Bookmarks
  registry->RegisterBooleanPref(prefs::kIosBookmarkPromoAlreadySeen, false);
  registry->RegisterIntegerPref(prefs::kIosBookmarkSigninPromoDisplayedCount,
                                0);
  // NTP Feed
  registry->RegisterBooleanPref(prefs::kIosNtpFeedTopPromoAlreadySeen, false);
  registry->RegisterIntegerPref(prefs::kIosNtpFeedTopSigninPromoDisplayedCount,
                                0);
  // Reading List
  registry->RegisterBooleanPref(prefs::kIosReadingListPromoAlreadySeen, false);
  registry->RegisterIntegerPref(prefs::kIosReadingListSigninPromoDisplayedCount,
                                0);
}

+ (BOOL)shouldDisplaySigninPromoViewWithAccessPoint:
            (signin_metrics::AccessPoint)accessPoint
                              authenticationService:
                                  (AuthenticationService*)authenticationService
                                        prefService:(PrefService*)prefService {
  // Checks if user can't sign in.
  switch (authenticationService->GetServiceStatus()) {
    case AuthenticationService::ServiceStatus::SigninForcedByPolicy:
    case AuthenticationService::ServiceStatus::SigninAllowed:
      // The user is allowed to sign-in, so the sign-in/sync promo can be
      // displayed.
      break;
    case AuthenticationService::ServiceStatus::SigninDisabledByUser:
    case AuthenticationService::ServiceStatus::SigninDisabledByPolicy:
    case AuthenticationService::ServiceStatus::SigninDisabledByInternal:
      // The user is not allowed to sign-in. The promo cannot be displayed.
      return NO;
  }

  // Always show the feed signin promo if the experimental setting is enabled.
  if (accessPoint ==
          signin_metrics::AccessPoint::ACCESS_POINT_NTP_FEED_TOP_PROMO &&
      experimental_flags::ShouldForceFeedSigninPromo()) {
    return YES;
  }

  // Checks if the user has exceeded the max impression count.
  const int maxDisplayedCount =
      accessPoint ==
              signin_metrics::AccessPoint::ACCESS_POINT_NTP_FEED_TOP_PROMO
          ? FeedSyncPromoAutodismissCount()
          : kAutomaticSigninPromoViewDismissCount;
  const char* displayedCountPreferenceKey =
      DisplayedCountPreferenceKey(accessPoint);
  const int displayedCount =
      prefService ? prefService->GetInteger(displayedCountPreferenceKey)
                  : INT_MAX;
  if (displayedCount >= maxDisplayedCount) {
    return NO;
  }

  // For the top-of-feed promo, the user must have engaged with a feed first.
  if (accessPoint ==
          signin_metrics::AccessPoint::ACCESS_POINT_NTP_FEED_TOP_PROMO &&
      (![[NSUserDefaults standardUserDefaults]
           boolForKey:kEngagedWithFeedKey] ||
       ShouldIgnoreFeedEngagementConditionForTopSyncPromo())) {
    return NO;
  }

  // Checks if user has already acknowledged or dismissed the promo.
  const char* alreadySeenSigninViewPreferenceKey =
      AlreadySeenSigninViewPreferenceKey(accessPoint);
  if (alreadySeenSigninViewPreferenceKey && prefService &&
      prefService->GetBoolean(alreadySeenSigninViewPreferenceKey)) {
    return NO;
  }

  // If no conditions are met, show the promo.
  return YES;
}

- (instancetype)
    initWithAccountManagerService:
        (ChromeAccountManagerService*)accountManagerService
                      authService:(AuthenticationService*)authService
                      prefService:(PrefService*)prefService
                      syncService:(syncer::SyncService*)syncService
                      accessPoint:(signin_metrics::AccessPoint)accessPoint
                        presenter:(id<SigninPresenter>)presenter
               baseViewController:(UIViewController*)baseViewController {
  self = [super init];
  if (self) {
    DCHECK(accountManagerService);
    DCHECK(IsSupportedAccessPoint(accessPoint));
    _accountManagerService = accountManagerService;
    _authService = authService;
    _prefService = prefService;
    _syncService = syncService;
    _accessPoint = accessPoint;
    _dataTypeToWaitForInitialSync = syncer::ModelType::UNSPECIFIED;
    _presenter = presenter;
    _baseViewController = baseViewController;
    _accountManagerServiceObserver =
        std::make_unique<ChromeAccountManagerServiceObserverBridge>(
            self, _accountManagerService);
    // Starting the sync state observation enables the sign-in progress to be
    // set to YES even if the user hasn't interacted with the promo. It is
    // intentional to keep UX consistency, given the initial sync cancellation
    // which should end the sign-in progress is tricky to detect.
    _syncObserverBridge =
        std::make_unique<SyncObserverBridge>(self, _syncService);

    id<SystemIdentity> displayedIdentity =
        GetDisplayedIdentity(self.authService, self.accountManagerService);
    if (displayedIdentity) {
      self.displayedIdentity = displayedIdentity;
    }
  }
  return self;
}

- (void)dealloc {
  DCHECK_EQ(SigninPromoViewState::kInvalid, _signinPromoViewState)
      << base::SysNSStringToUTF8([self description]);
}

- (SigninPromoViewConfigurator*)createConfigurator {
  BOOL hasCloseButton =
      AlreadySeenSigninViewPreferenceKey(self.accessPoint) != nullptr;
  if (self.authService->HasPrimaryIdentity(signin::ConsentLevel::kSignin)) {
    if (!self.displayedIdentity) {
      // TODO(crbug.com/1227708): The default identity should already be known
      // by the mediator. We should not have no identity. This can be reproduced
      // with EGtests with bots. The identity notification might not have
      // received yet. Let's update the promo identity.
      [self identityListChanged];
    }
    DCHECK(self.displayedIdentity)
        << base::SysNSStringToUTF8([self description]);
    return [[SigninPromoViewConfigurator alloc]
        initWithSigninPromoViewMode:SigninPromoViewModeSyncWithPrimaryAccount
                          userEmail:self.displayedIdentity.userEmail
                      userGivenName:self.displayedIdentity.userGivenName
                          userImage:self.displayedIdentityAvatar
                     hasCloseButton:hasCloseButton
                   hasSignInSpinner:self.showSpinner];
  }
  if (self.displayedIdentity) {
    return [[SigninPromoViewConfigurator alloc]
        initWithSigninPromoViewMode:SigninPromoViewModeSigninWithAccount
                          userEmail:self.displayedIdentity.userEmail
                      userGivenName:self.displayedIdentity.userGivenName
                          userImage:self.displayedIdentityAvatar
                     hasCloseButton:hasCloseButton
                   hasSignInSpinner:self.showSpinner];
  }
  SigninPromoViewConfigurator* configurator =
      [[SigninPromoViewConfigurator alloc]
          initWithSigninPromoViewMode:SigninPromoViewModeNoAccounts
                            userEmail:nil
                        userGivenName:nil
                            userImage:nil
                       hasCloseButton:hasCloseButton
                     hasSignInSpinner:self.showSpinner];
  switch (self.signinPromoAction) {
    case SigninPromoAction::kSync:
      break;
    case SigninPromoAction::kSigninSheet:
    case SigninPromoAction::kInstantSignin:
      configurator.primaryButtonTitleNoAccountsModeOverride =
          l10n_util::GetNSString(IDS_IOS_CONSISTENCY_PROMO_SIGN_IN);
      break;
  }
  return configurator;
}

- (void)signinPromoViewIsVisible {
  DCHECK(!self.invalidOrClosed) << base::SysNSStringToUTF8([self description]);
  if (self.signinPromoViewVisible) {
    return;
  }
  if (self.signinPromoViewState == SigninPromoViewState::kNeverVisible) {
    self.signinPromoViewState = SigninPromoViewState::kUnused;
  }
  self.signinPromoViewVisible = YES;
  signin_metrics::RecordSigninImpressionUserActionForAccessPoint(
      self.accessPoint);
  const char* displayedCountPreferenceKey =
      DisplayedCountPreferenceKey(self.accessPoint);
  if (!displayedCountPreferenceKey) {
    return;
  }

  int displayedCount =
      self.prefService->GetInteger(displayedCountPreferenceKey);

  // For the top-of-feed promo, we only record 1 impression per session. For all
  // other promos, we record 1 impression per view.
  if (self.accessPoint ==
      signin_metrics::AccessPoint::ACCESS_POINT_NTP_FEED_TOP_PROMO) {
    NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
    NSDate* lastImpressionIncrementedDate =
        [defaults objectForKey:kLastSigninImpressionTopOfFeedKey];

    // If no impression has been logged, or if the last log was beyond the
    // session time, log the current time and increment the count.
    if (!lastImpressionIncrementedDate ||
        [[NSDate date] timeIntervalSinceDate:lastImpressionIncrementedDate] >=
            kTopOfFeedSessionTimeInterval) {
      [defaults setObject:[NSDate date]
                   forKey:kLastSigninImpressionTopOfFeedKey];
      ++displayedCount;
      self.prefService->SetInteger(displayedCountPreferenceKey, displayedCount);
    }
  } else {
    ++displayedCount;
    self.prefService->SetInteger(displayedCountPreferenceKey, displayedCount);
  }
}

- (void)signinPromoViewIsHidden {
  DCHECK(!self.invalidOrClosed) << base::SysNSStringToUTF8([self description]);
  self.signinPromoViewVisible = NO;
}

- (void)disconnect {
  [self signinPromoViewIsRemoved];
  self.consumer = nil;
  self.accountManagerService = nullptr;
  self.authService = nullptr;
  _syncService = nullptr;
  _accountManagerServiceObserver.reset();
  _syncObserverBridge.reset();
}

#pragma mark - Public properties

- (BOOL)isInvalidClosedOrNeverVisible {
  return self.invalidOrClosed ||
         self.signinPromoViewState == SigninPromoViewState::kNeverVisible;
}

- (BOOL)showSpinner {
  return self.signinInProgress || self.initialSyncInProgress;
}

#pragma mark - Private properties

- (BOOL)isInvalidOrClosed {
  return self.signinPromoViewState == SigninPromoViewState::kClosed ||
         self.signinPromoViewState == SigninPromoViewState::kInvalid;
}

// Sets the Chrome identity to display in the sign-in promo.
- (void)setDisplayedIdentity:(id<SystemIdentity>)identity {
  _displayedIdentity = identity;
  if (!_displayedIdentity) {
    self.displayedIdentityAvatar = nil;
  } else {
    self.displayedIdentityAvatar =
        self.accountManagerService->GetIdentityAvatarWithIdentity(
            _displayedIdentity, IdentityAvatarSize::SmallSize);
  }
}

// Updates `_signinInProgress` value, and sends a notification the consumer
// to update the sign-in promo, so the progress indicator can be displayed.
- (void)setSigninInProgress:(BOOL)signinInProgress {
  if (_signinInProgress == signinInProgress) {
    return;
  }
  _signinInProgress = signinInProgress;
  SigninPromoViewConfigurator* configurator = [self createConfigurator];
  if ([self.consumer
          respondsToSelector:@selector(promoProgressStateDidChange)]) {
    [self.consumer promoProgressStateDidChange];
  }
  [self.consumer configureSigninPromoWithConfigurator:configurator
                                      identityChanged:NO];
}

- (void)setInitialSyncInProgress:(BOOL)initialSyncInProgress {
  if (_initialSyncInProgress == initialSyncInProgress) {
    return;
  }
  _initialSyncInProgress = initialSyncInProgress;
  SigninPromoViewConfigurator* configurator = [self createConfigurator];
  if ([self.consumer
          respondsToSelector:@selector(promoProgressStateDidChange)]) {
    [self.consumer promoProgressStateDidChange];
  }
  [self.consumer configureSigninPromoWithConfigurator:configurator
                                      identityChanged:NO];
}

- (void)setSigninPromoAction:(SigninPromoAction)signinPromoAction {
  if (_signinPromoAction == signinPromoAction) {
    return;
  }
  _signinPromoAction = signinPromoAction;
  SigninPromoViewConfigurator* configurator = [self createConfigurator];
  [self.consumer configureSigninPromoWithConfigurator:configurator
                                      identityChanged:NO];
}

#pragma mark - Private

// Sends the update notification to the consummer if the signin-in is not in
// progress. This is to avoid to update the sign-in promo view in the
// background.
- (void)sendConsumerNotificationWithIdentityChanged:(BOOL)identityChanged {
  if (self.showSpinner) {
    return;
  }
  SigninPromoViewConfigurator* configurator = [self createConfigurator];
  [self.consumer configureSigninPromoWithConfigurator:configurator
                                      identityChanged:identityChanged];
}

// Records in histogram, the number of time the sign-in promo is displayed
// before the sign-in button is pressed, if the current access point supports
// it.
- (void)sendImpressionsTillSigninButtonsHistogram {
  DCHECK(!self.invalidClosedOrNeverVisible)
      << base::SysNSStringToUTF8([self description]);
  const char* displayedCountPreferenceKey =
      DisplayedCountPreferenceKey(self.accessPoint);
  if (!displayedCountPreferenceKey)
    return;
  int displayedCount =
      self.prefService->GetInteger(displayedCountPreferenceKey);
  RecordImpressionsTilSigninButtonsHistogramForAccessPoint(self.accessPoint,
                                                           displayedCount);
}

// Finishes the sign-in process.
- (void)signinCallbackWithResult:(SigninCoordinatorResult)result {
  if (self.signinPromoViewState == SigninPromoViewState::kInvalid) {
    // The mediator owner can remove the view before the sign-in is done.
    return;
  }
  // We can turn on `self.initialSyncInProgress`, if the sign-in is successful.
  // We can't call now GetTypesWithPendingDownloadForInitialSync() related to
  // a post task issue.
  self.initialSyncInProgress = (result == SigninCoordinatorResultSuccess) &&
                               [self shouldWaitForInitialSync];
  DCHECK_EQ(SigninPromoViewState::kUsedAtLeastOnce, self.signinPromoViewState)
      << base::SysNSStringToUTF8([self description]);
  DCHECK(self.signinInProgress) << base::SysNSStringToUTF8([self description]);
  self.signinInProgress = NO;
}

// Starts sign-in process with the Chrome identity from `identity`.
- (void)showSigninWithIdentity:(id<SystemIdentity>)identity
                     operation:(AuthenticationOperation)operation
                   promoAction:(signin_metrics::PromoAction)promoAction {
  self.signinPromoViewState = SigninPromoViewState::kUsedAtLeastOnce;
  self.signinInProgress = YES;
  __weak SigninPromoViewMediator* weakSelf = self;
  // This mediator might be removed before the sign-in callback is invoked.
  // (if the owner receive primary account notification).
  // To make sure -[<SigninPromoViewConsumer> signinDidFinish], we have to save
  // in a variable and not get it from weakSelf (that might not exist anymore).
  __weak id<SigninPromoViewConsumer> weakConsumer = self.consumer;
  ShowSigninCommandCompletionCallback completion =
      ^(SigninCoordinatorResult result, SigninCompletionInfo* completionInfo) {
        [weakSelf signinCallbackWithResult:result];
        if ([weakConsumer respondsToSelector:@selector(signinDidFinish)]) {
          [weakConsumer signinDidFinish];
        }
      };
  ShowSigninCommand* command =
      [[ShowSigninCommand alloc] initWithOperation:operation
                                          identity:identity
                                       accessPoint:self.accessPoint
                                       promoAction:promoAction
                                          callback:completion];
  [self.presenter showSignin:command];
}

// Changes the promo view state, and records the metrics.
- (void)signinPromoViewIsRemoved {
  DCHECK_NE(SigninPromoViewState::kInvalid, self.signinPromoViewState)
      << base::SysNSStringToUTF8([self description]);
  BOOL wasNeverVisible =
      self.signinPromoViewState == SigninPromoViewState::kNeverVisible;
  BOOL wasUnused = self.signinPromoViewState == SigninPromoViewState::kUnused;
  self.signinPromoViewState = SigninPromoViewState::kInvalid;
  self.signinPromoViewVisible = NO;
  if (wasNeverVisible)
    return;

  // If the sign-in promo view has been used at least once, it should not be
  // counted as dismissed (even if the sign-in has been canceled).
  const char* displayedCountPreferenceKey =
      DisplayedCountPreferenceKey(self.accessPoint);
  if (!displayedCountPreferenceKey || !wasUnused)
    return;

  // If the sign-in view is removed when the user is authenticated, then the
  // sign-in for sync has been done by another view, and this mediator cannot be
  // counted as being dismissed.
  // TODO(crbug.com/1462858): Once new sync opt-ins are deprecated this usage
  // of kSync will become obsolete. Delete this code after phase 2.
  if (self.authService->HasPrimaryIdentity(signin::ConsentLevel::kSync))
    return;
  int displayedCount =
      self.prefService->GetInteger(displayedCountPreferenceKey);
  RecordImpressionsTilDismissHistogramForAccessPoint(self.accessPoint,
                                                     displayedCount);
}

// Whether the sign-in needs to wait for the end of the initial sync to
// complete.
- (BOOL)shouldWaitForInitialSync {
  return self.dataTypeToWaitForInitialSync != syncer::ModelType::UNSPECIFIED;
}

- (void)optInBookmarkReadingListAccountStorage {
  bool bookmarksAccountStorageEnabled =
      base::FeatureList::IsEnabled(syncer::kEnableBookmarksAccountStorage);
  bool dualReadingListModelEnabled = base::FeatureList::IsEnabled(
      syncer::kReadingListEnableDualReadingListModel);
  bool readingListTransportUponSignInEnabled = base::FeatureList::IsEnabled(
      syncer::kReadingListEnableSyncTransportModeUponSignIn);
  CHECK(bookmarksAccountStorageEnabled ||
        (dualReadingListModelEnabled && readingListTransportUponSignInEnabled))
      << "bookmarksAccountStorageEnabled: " << bookmarksAccountStorageEnabled
      << ", dualReadingListModelEnabled: " << dualReadingListModelEnabled
      << ", readingListTransportUponSignInEnabled: "
      << readingListTransportUponSignInEnabled;
  _syncService->GetUserSettings()
      ->SetBookmarksAndReadingListAccountStorageOptIn(true);
}

#pragma mark - ChromeAccountManagerServiceObserver

- (void)identityListChanged {
  id<SystemIdentity> currentIdentity = self.displayedIdentity;
  id<SystemIdentity> displayedIdentity =
      GetDisplayedIdentity(self.authService, self.accountManagerService);
  if (![currentIdentity isEqual:displayedIdentity]) {
    // Don't update the the sign-in promo if the sign-in is in progress,
    // to avoid flashes of the promo.
    self.displayedIdentity = displayedIdentity;
    [self sendConsumerNotificationWithIdentityChanged:YES];
  }
}

- (void)identityUpdated:(id<SystemIdentity>)identity {
  [self sendConsumerNotificationWithIdentityChanged:NO];
}

- (void)onChromeAccountManagerServiceShutdown:
    (ChromeAccountManagerService*)accountManagerService {
  // TODO(crbug.com/1489595): Remove `[self disconnect]`.
  [self disconnect];
}

#pragma mark - SigninPromoViewDelegate

- (void)signinPromoViewDidTapSigninWithNewAccount:
    (SigninPromoView*)signinPromoView {
  DCHECK(!self.displayedIdentity)
      << base::SysNSStringToUTF8([self description]);
  // The promo on top of the feed is only logged as visible when most of it can
  // be seen, so it can be used without `self.signinPromoViewVisible`.
  DCHECK(self.signinPromoViewVisible ||
         self.accessPoint ==
             signin_metrics::AccessPoint::ACCESS_POINT_NTP_FEED_TOP_PROMO)
      << base::SysNSStringToUTF8([self description]);
  DCHECK(!self.invalidClosedOrNeverVisible)
      << base::SysNSStringToUTF8([self description]);
  [self sendImpressionsTillSigninButtonsHistogram];
  // On iOS, the promo does not have a button to add and account when there is
  // already an account on the device. That flow goes through the NOT_DEFAULT
  // promo instead. Always use the NO_EXISTING_ACCOUNT variant.
  signin_metrics::PromoAction promoAction =
      signin_metrics::PromoAction::PROMO_ACTION_NEW_ACCOUNT_NO_EXISTING_ACCOUNT;
  signin_metrics::RecordSigninUserActionForAccessPoint(self.accessPoint);
  switch (self.signinPromoAction) {
    case SigninPromoAction::kInstantSignin:
      [self showSigninWithIdentity:nil
                         operation:AuthenticationOperation::kInstantSignin
                       promoAction:promoAction];
      return;
    case SigninPromoAction::kSync:
      [self showSigninWithIdentity:nil
                         operation:AuthenticationOperation::kSigninAndSync
                       promoAction:promoAction];
      return;
    case SigninPromoAction::kSigninSheet:
      [self showSigninWithIdentity:nil
                         operation:AuthenticationOperation::kSigninOnly
                       promoAction:promoAction];
      return;
  }
}

- (void)signinPromoViewDidTapSigninWithDefaultAccount:
    (SigninPromoView*)signinPromoView {
  DCHECK(self.displayedIdentity) << base::SysNSStringToUTF8([self description]);
  DCHECK(self.signinPromoViewVisible)
      << base::SysNSStringToUTF8([self description]);
  DCHECK(!self.invalidClosedOrNeverVisible)
      << base::SysNSStringToUTF8([self description]);
  [self sendImpressionsTillSigninButtonsHistogram];
  switch (self.signinPromoAction) {
    case SigninPromoAction::kInstantSignin:
      [self showSigninWithIdentity:self.displayedIdentity
                         operation:AuthenticationOperation::kInstantSignin
                       promoAction:signin_metrics::PromoAction::
                                       PROMO_ACTION_WITH_DEFAULT];
      return;
    case SigninPromoAction::kSync:
      [self showSigninWithIdentity:self.displayedIdentity
                         operation:AuthenticationOperation::kSigninAndSync
                       promoAction:signin_metrics::PromoAction::
                                       PROMO_ACTION_WITH_DEFAULT];
      return;
    case SigninPromoAction::kSigninSheet:
      [self showSigninWithIdentity:nil
                         operation:AuthenticationOperation::kSigninOnly
                       promoAction:signin_metrics::PromoAction::
                                       PROMO_ACTION_WITH_DEFAULT];
      return;
  }
}

- (void)signinPromoViewDidTapSigninWithOtherAccount:
    (SigninPromoView*)signinPromoView {
  DCHECK(self.displayedIdentity) << base::SysNSStringToUTF8([self description]);
  DCHECK(self.signinPromoViewVisible)
      << base::SysNSStringToUTF8([self description]);
  DCHECK(!self.invalidClosedOrNeverVisible)
      << base::SysNSStringToUTF8([self description]);
  [self sendImpressionsTillSigninButtonsHistogram];
  signin_metrics::RecordSigninUserActionForAccessPoint(self.accessPoint);

  switch (self.signinPromoAction) {
    case SigninPromoAction::kInstantSignin:
      [self showSigninWithIdentity:nil
                         operation:AuthenticationOperation::kInstantSignin
                       promoAction:signin_metrics::PromoAction::
                                       PROMO_ACTION_NOT_DEFAULT];
      return;
    case SigninPromoAction::kSync:
      [self showSigninWithIdentity:nil
                         operation:AuthenticationOperation::kSigninAndSync
                       promoAction:signin_metrics::PromoAction::
                                       PROMO_ACTION_NOT_DEFAULT];
      return;
    case SigninPromoAction::kSigninSheet:
      [self showSigninWithIdentity:nil
                         operation:AuthenticationOperation::kSigninOnly
                       promoAction:signin_metrics::PromoAction::
                                       PROMO_ACTION_NOT_DEFAULT];
      return;
  }
}

- (void)signinPromoViewCloseButtonWasTapped:(SigninPromoView*)view {
  DCHECK(!self.invalidClosedOrNeverVisible)
      << base::SysNSStringToUTF8([self description]);
  // The promo on top of the feed is only logged as visible when most of it can
  // be seen, so it can be dismissed without `self.signinPromoViewVisible`.
  DCHECK(self.signinPromoViewVisible ||
         self.accessPoint ==
             signin_metrics::AccessPoint::ACCESS_POINT_NTP_FEED_TOP_PROMO)
      << base::SysNSStringToUTF8([self description]);
  self.signinPromoViewState = SigninPromoViewState::kClosed;
  const char* alreadySeenSigninViewPreferenceKey =
      AlreadySeenSigninViewPreferenceKey(self.accessPoint);
  DCHECK(alreadySeenSigninViewPreferenceKey)
      << base::SysNSStringToUTF8([self description]);
  self.prefService->SetBoolean(alreadySeenSigninViewPreferenceKey, true);
  const char* displayedCountPreferenceKey =
      DisplayedCountPreferenceKey(self.accessPoint);
  if (displayedCountPreferenceKey) {
    int displayedCount =
        self.prefService->GetInteger(displayedCountPreferenceKey);
    RecordImpressionsTilXButtonHistogramForAccessPoint(self.accessPoint,
                                                       displayedCount);
  }
  if ([self.consumer respondsToSelector:@selector
                     (signinPromoViewMediatorCloseButtonWasTapped:)]) {
    [self.consumer signinPromoViewMediatorCloseButtonWasTapped:self];
  }
}

#pragma mark - SyncObserverModelBridge

- (void)onSyncStateChanged {
  self.initialSyncInProgress =
      [self shouldWaitForInitialSync] &&
      _syncService->GetTypesWithPendingDownloadForInitialSync().Has(
          self.dataTypeToWaitForInitialSync);
}

#pragma mark - NSObject

- (NSString*)description {
  return [NSString
      stringWithFormat:
          @"<%@: %p, identity: %p, signinPromoViewState: %d, "
          @"signinInProgress: %d, initialSyncInProgress %d, accessPoint: %d, "
          @"signinPromoViewVisible: %d, invalidOrClosed %d>",
          self.class.description, self, self.displayedIdentity,
          static_cast<int>(self.signinPromoViewState), self.signinInProgress,
          self.initialSyncInProgress, static_cast<int>(self.accessPoint),
          self.signinPromoViewVisible, self.invalidOrClosed];
}

@end
