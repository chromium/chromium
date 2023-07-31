// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin_promo_view_mediator.h"

#import <memory>

#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/discover_feed/feed_constants.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_browser_agent.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_observer_bridge.h"
#import "ios/chrome/browser/signin/system_identity.h"
#import "ios/chrome/browser/sync/sync_observer_bridge.h"
#import "ios/chrome/browser/ui/authentication/authentication_flow.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_configurator.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_consumer.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_coordinator.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_utils.h"
#import "ios/chrome/browser/ui/authentication/signin_presenter.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/identity_chooser/identity_chooser_coordinator.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/identity_chooser/identity_chooser_coordinator_delegate.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_feature.h"
#import "ios/chrome/browser/ui/scoped_ui_blocker/scoped_ui_blocker.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
    case signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS:
    case signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_MANAGER:
    case signin_metrics::AccessPoint::ACCESS_POINT_RECENT_TABS:
    case signin_metrics::AccessPoint::ACCESS_POINT_TAB_SWITCHER:
    case signin_metrics::AccessPoint::ACCESS_POINT_NTP_FEED_TOP_PROMO:
    case signin_metrics::AccessPoint::
        ACCESS_POINT_POST_DEVICE_RESTORE_SIGNIN_PROMO:
    case signin_metrics::AccessPoint::ACCESS_POINT_NTP_FEED_CARD_MENU_PROMO:
    case signin_metrics::AccessPoint::ACCESS_POINT_NTP_FEED_BOTTOM_PROMO:
    case signin_metrics::AccessPoint::ACCESS_POINT_READING_LIST:
      return true;
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
    case signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS:
      base::UmaHistogramCounts100(
          "MobileSignInPromo.SettingsManager.ImpressionsTilSigninButtons",
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
    case signin_metrics::AccessPoint::ACCESS_POINT_MAX:
    case signin_metrics::AccessPoint::ACCESS_POINT_REAUTH_INFO_BAR:
    case signin_metrics::AccessPoint::ACCESS_POINT_ACCOUNT_CONSISTENCY_SERVICE:
    case signin_metrics::AccessPoint::ACCESS_POINT_SEARCH_COMPANION:
    case signin_metrics::AccessPoint::ACCESS_POINT_SET_UP_LIST:
    case signin_metrics::AccessPoint::
        ACCESS_POINT_PASSWORD_MIGRATION_WARNING_ANDROID:
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
    case signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS:
      base::UmaHistogramCounts100(
          "MobileSignInPromo.SettingsManager.ImpressionsTilDismiss",
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
    case signin_metrics::AccessPoint::ACCESS_POINT_MAX:
    case signin_metrics::AccessPoint::ACCESS_POINT_REAUTH_INFO_BAR:
    case signin_metrics::AccessPoint::ACCESS_POINT_ACCOUNT_CONSISTENCY_SERVICE:
    case signin_metrics::AccessPoint::ACCESS_POINT_SEARCH_COMPANION:
    case signin_metrics::AccessPoint::ACCESS_POINT_SET_UP_LIST:
    case signin_metrics::AccessPoint::
        ACCESS_POINT_PASSWORD_MIGRATION_WARNING_ANDROID:
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
    case signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS:
      base::UmaHistogramCounts100(
          "MobileSignInPromo.SettingsManager.ImpressionsTilXButton",
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
    case signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS:
      return prefs::kIosSettingsSigninPromoDisplayedCount;
    case signin_metrics::AccessPoint::ACCESS_POINT_NTP_FEED_TOP_PROMO:
      return prefs::kIosNtpFeedTopSigninPromoDisplayedCount;
    case signin_metrics::AccessPoint::ACCESS_POINT_READING_LIST:
      return prefs::kIosReadingListSigninPromoDisplayedCount;
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
    case signin_metrics::AccessPoint::ACCESS_POINT_MAX:
    case signin_metrics::AccessPoint::ACCESS_POINT_REAUTH_INFO_BAR:
    case signin_metrics::AccessPoint::ACCESS_POINT_ACCOUNT_CONSISTENCY_SERVICE:
    case signin_metrics::AccessPoint::ACCESS_POINT_SEARCH_COMPANION:
    case signin_metrics::AccessPoint::ACCESS_POINT_SET_UP_LIST:
    case signin_metrics::AccessPoint::
        ACCESS_POINT_PASSWORD_MIGRATION_WARNING_ANDROID:
      return nullptr;
  }
}

// Returns AlreadySeen preference key string for `access_point`.
const char* AlreadySeenSigninViewPreferenceKey(
    signin_metrics::AccessPoint access_point) {
  switch (access_point) {
    case signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_MANAGER:
      return prefs::kIosBookmarkPromoAlreadySeen;
    case signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS:
      return prefs::kIosSettingsPromoAlreadySeen;
    case signin_metrics::AccessPoint::ACCESS_POINT_NTP_FEED_TOP_PROMO:
      return prefs::kIosNtpFeedTopPromoAlreadySeen;
    case signin_metrics::AccessPoint::ACCESS_POINT_READING_LIST:
      return prefs::kIosReadingListPromoAlreadySeen;
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
    case signin_metrics::AccessPoint::ACCESS_POINT_MAX:
    case signin_metrics::AccessPoint::ACCESS_POINT_REAUTH_INFO_BAR:
    case signin_metrics::AccessPoint::ACCESS_POINT_ACCOUNT_CONSISTENCY_SERVICE:
    case signin_metrics::AccessPoint::ACCESS_POINT_SEARCH_COMPANION:
    case signin_metrics::AccessPoint::ACCESS_POINT_SET_UP_LIST:
    case signin_metrics::AccessPoint::
        ACCESS_POINT_PASSWORD_MIGRATION_WARNING_ANDROID:
      return nullptr;
  }
}

}  // namespace

@interface SigninPromoViewMediator () <ChromeAccountManagerServiceObserver,
                                       IdentityChooserCoordinatorDelegate,
                                       SyncObserverModelBridge>

// Redefined to be readwrite.
@property(nonatomic, strong, readwrite) id<SystemIdentity> identity;
@property(nonatomic, assign, readwrite, getter=isSigninInProgress)
    BOOL signinInProgress;

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

// Identity avatar.
@property(nonatomic, strong) UIImage* identityAvatar;

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
  Browser* _browser;
  // View used to present sign-in UI.
  UIViewController* _baseViewController;
  // Sign-in flow, used only when `self.signInOnly` is `YES`.
  AuthenticationFlow* _authenticationFlow;
  // Sync service.
  syncer::SyncService* _syncService;
  // Observer for changes to the sync state.
  std::unique_ptr<SyncObserverBridge> _syncObserverBridge;
  // Coordinator for the user to select an account.
  IdentityChooserCoordinator* _identityChooserCoordinator;
  // Coordinator to add an account.
  SigninCoordinator* _signinCoordinator;
  // TODO(crbug.com/1448830): This class should not need to block the UI.
  // The UI blocker is only used in sign-in only cases.
  std::unique_ptr<ScopedUIBlocker> _uiBlocker;
  // The type of data that should be synced before the sign-in completes.
  syncer::ModelType _dataTypeToWaitForInitialSync;
}

+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry {
  // Bookmarks
  registry->RegisterBooleanPref(prefs::kIosBookmarkPromoAlreadySeen, false);
  registry->RegisterIntegerPref(prefs::kIosBookmarkSigninPromoDisplayedCount,
                                0);
  // Settings
  registry->RegisterBooleanPref(prefs::kIosSettingsPromoAlreadySeen, false);
  registry->RegisterIntegerPref(prefs::kIosSettingsSigninPromoDisplayedCount,
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

- (instancetype)initWithBrowser:(Browser*)browser
          accountManagerService:
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
    _browser = browser;
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

    id<SystemIdentity> defaultIdentity = [self defaultIdentity];
    if (defaultIdentity) {
      self.identity = defaultIdentity;
    }
  }
  return self;
}

- (void)dealloc {
  DCHECK_EQ(ios::SigninPromoViewState::Invalid, _signinPromoViewState)
      << base::SysNSStringToUTF8([self description]);
}

- (SigninPromoViewConfigurator*)createConfigurator {
  BOOL hasCloseButton =
      AlreadySeenSigninViewPreferenceKey(self.accessPoint) != nullptr;
  if (self.authService->HasPrimaryIdentity(signin::ConsentLevel::kSignin)) {
    if (!self.identity) {
      // TODO(crbug.com/1227708): The default identity should already be known
      // by the mediator. We should not have no identity. This can be reproduced
      // with EGtests with bots. The identity notification might not have
      // received yet. Let's update the promo identity.
      [self identityListChanged];
    }
    DCHECK(self.identity) << base::SysNSStringToUTF8([self description]);
    return [[SigninPromoViewConfigurator alloc]
        initWithSigninPromoViewMode:SigninPromoViewModeSyncWithPrimaryAccount
                          userEmail:self.identity.userEmail
                      userGivenName:self.identity.userGivenName
                          userImage:self.identityAvatar
                     hasCloseButton:hasCloseButton
                   hasSignInSpinner:self.signinInProgress];
  }
  if (self.identity) {
    return [[SigninPromoViewConfigurator alloc]
        initWithSigninPromoViewMode:SigninPromoViewModeSigninWithAccount
                          userEmail:self.identity.userEmail
                      userGivenName:self.identity.userGivenName
                          userImage:self.identityAvatar
                     hasCloseButton:hasCloseButton
                   hasSignInSpinner:self.signinInProgress];
  }
  SigninPromoViewConfigurator* configurator =
      [[SigninPromoViewConfigurator alloc]
          initWithSigninPromoViewMode:SigninPromoViewModeNoAccounts
                            userEmail:nil
                        userGivenName:nil
                            userImage:nil
                       hasCloseButton:hasCloseButton
                     hasSignInSpinner:self.signinInProgress];
  if (self.signInOnly) {
    configurator.primaryButtonTitleNoAccountsModeOverride =
        l10n_util::GetNSString(IDS_IOS_CONSISTENCY_PROMO_SIGN_IN);
  }
  return configurator;
}

- (void)signinPromoViewIsVisible {
  DCHECK(!self.invalidOrClosed) << base::SysNSStringToUTF8([self description]);
  if (self.signinPromoViewVisible) {
    return;
  }
  if (self.signinPromoViewState == ios::SigninPromoViewState::NeverVisible) {
    self.signinPromoViewState = ios::SigninPromoViewState::Unused;
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

- (void)setDataTypeToWaitForInitialSync:(syncer::ModelType)dataType {
  _dataTypeToWaitForInitialSync = dataType;
  [self updateSignInProgressWithSyncState];
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
         self.signinPromoViewState == ios::SigninPromoViewState::NeverVisible;
}

#pragma mark - Private properties

- (BOOL)isInvalidOrClosed {
  return self.signinPromoViewState == ios::SigninPromoViewState::Closed ||
         self.signinPromoViewState == ios::SigninPromoViewState::Invalid;
}

#pragma mark - Private

// Returns the identity for the sync promo. This should be the signed in promo,
// if the user is signed in. If not signed in, the default identity from
// AccountManagerService.
- (id<SystemIdentity>)defaultIdentity {
  if (self.authService->HasPrimaryIdentity(signin::ConsentLevel::kSignin)) {
    return self.authService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  }
  DCHECK(self.accountManagerService)
      << base::SysNSStringToUTF8([self description]);
  return self.accountManagerService->GetDefaultIdentity();
}

// Sets the Chrome identity to display in the sign-in promo.
- (void)setIdentity:(id<SystemIdentity>)identity {
  _identity = identity;
  if (!_identity) {
    self.identityAvatar = nil;
  } else {
    self.identityAvatar =
        self.accountManagerService->GetIdentityAvatarWithIdentity(
            _identity, IdentityAvatarSize::SmallSize);
  }
}

// Sends the update notification to the consummer if the signin-in is not in
// progress. This is to avoid to update the sign-in promo view in the
// background.
- (void)sendConsumerNotificationWithIdentityChanged:(BOOL)identityChanged {
  if (self.signinInProgress)
    return;
  SigninPromoViewConfigurator* configurator = [self createConfigurator];
  [self.consumer configureSigninPromoWithConfigurator:configurator
                                      identityChanged:identityChanged];
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

- (void)setSignInInOnly:(BOOL)signInOnly {
  if (_signInOnly == signInOnly) {
    return;
  }
  _signInOnly = signInOnly;
  SigninPromoViewConfigurator* configurator = [self createConfigurator];
  [self.consumer configureSigninPromoWithConfigurator:configurator
                                      identityChanged:NO];
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
- (void)signinCallback {
  if (self.signinPromoViewState == ios::SigninPromoViewState::Invalid) {
    // The mediator owner can remove the view before the sign-in is done.
    return;
  }
  DCHECK_EQ(ios::SigninPromoViewState::UsedAtLeastOnce,
            self.signinPromoViewState)
      << base::SysNSStringToUTF8([self description]);
  DCHECK(self.signinInProgress) << base::SysNSStringToUTF8([self description]);
  self.signinInProgress = NO;
}

// Starts sign-in process with the Chrome identity from `identity`.
- (void)showSigninWithIdentity:(id<SystemIdentity>)identity
                   promoAction:(signin_metrics::PromoAction)promoAction {
  self.signinPromoViewState = ios::SigninPromoViewState::UsedAtLeastOnce;
  self.signinInProgress = YES;
  __weak SigninPromoViewMediator* weakSelf = self;
  // This mediator might be removed before the sign-in callback is invoked.
  // (if the owner receive primary account notification).
  // To make sure -[<SigninPromoViewConsumer> signinDidFinish], we have to save
  // in a variable and not get it from weakSelf (that might not exist anymore).
  __weak id<SigninPromoViewConsumer> weakConsumer = self.consumer;
  ShowSigninCommandCompletionCallback completion =
      ^(SigninCoordinatorResult result) {
        [weakSelf signinCallback];
        if ([weakConsumer respondsToSelector:@selector(signinDidFinish)]) {
          [weakConsumer signinDidFinish];
        }
      };
  if ([self.consumer respondsToSelector:@selector
                     (signinPromoViewMediator:shouldOpenSigninWithIdentity
                                                :promoAction:completion:)]) {
    [self.consumer signinPromoViewMediator:self
              shouldOpenSigninWithIdentity:identity
                               promoAction:promoAction
                                completion:completion];
  } else {
    ShowSigninCommand* command = [[ShowSigninCommand alloc]
        initWithOperation:AuthenticationOperationSigninAndSync
                 identity:identity
              accessPoint:self.accessPoint
              promoAction:promoAction
                 callback:completion];
    [self.presenter showSignin:command];
  }
}

// Triggers the primary action when `signInOnly` is at YES: starts sign-in flow.
- (void)primaryActionForSignInOnly {
  DCHECK(self.signInOnly) << base::SysNSStringToUTF8([self description]);
  SceneState* sceneState =
      SceneStateBrowserAgent::FromBrowser(_browser)->GetSceneState();
  _uiBlocker = std::make_unique<ScopedUIBlocker>(sceneState);
  signin_metrics::RecordSigninUserActionForAccessPoint(self.accessPoint);
  self.signinPromoViewState = ios::SigninPromoViewState::UsedAtLeastOnce;
  self.signinInProgress = YES;
  [self startSignInOnlyFlow];
}

// Triggers the secondary action when `signInOnly` is at YES: starts the
// add account dialog.
- (void)secondaryActionForSignInOnly {
  DCHECK(self.signInOnly) << base::SysNSStringToUTF8([self description]);
  SceneState* sceneState =
      SceneStateBrowserAgent::FromBrowser(_browser)->GetSceneState();
  _uiBlocker = std::make_unique<ScopedUIBlocker>(sceneState);
  signin_metrics::RecordSigninUserActionForAccessPoint(self.accessPoint);
  self.signinPromoViewState = ios::SigninPromoViewState::UsedAtLeastOnce;
  self.signinInProgress = YES;
  _identityChooserCoordinator = [[IdentityChooserCoordinator alloc]
      initWithBaseViewController:_baseViewController
                         browser:_browser];
  _identityChooserCoordinator.delegate = self;
  [_identityChooserCoordinator start];
}

// Starts the sign-in only flow.
- (void)startSignInOnlyFlow {
  signin_metrics::RecordSigninUserActionForAccessPoint(self.accessPoint);
  _authenticationFlow = [[AuthenticationFlow alloc]
               initWithBrowser:_browser
                      identity:self.identity
                   accessPoint:self.accessPoint
              postSignInAction:PostSignInAction::
                                   kEnableBookmarkReadingListAccountStorage
      presentingViewController:_baseViewController];
  __weak id<SigninPromoViewConsumer> weakConsumer = self.consumer;
  __weak __typeof(self) weakSelf = self;
  [_authenticationFlow startSignInWithCompletion:^(BOOL success) {
    [weakSelf signInFlowCompletedForSignInOnly];
    if ([weakSelf shouldWaitForInitialSync]) {
      return;
    }
    weakSelf.signinInProgress = NO;
    if ([weakConsumer respondsToSelector:@selector(signinDidFinish)]) {
      [weakConsumer signinDidFinish];
    }
  }];
}

// Called when the sign-in flow is over. This method should only be called
// when this is a sign-in only flow.
- (void)signInFlowCompletedForSignInOnly {
  DCHECK(self.signInOnly) << base::SysNSStringToUTF8([self description]);
  _uiBlocker.reset();
}

- (void)startAddAccountForSignInOnly {
  DCHECK(!_signinCoordinator)
      << base::SysNSStringToUTF8([_signinCoordinator description]) << " "
      << base::SysNSStringToUTF8([self description]);
  DCHECK(self.signInOnly) << base::SysNSStringToUTF8([self description]);
  SceneState* sceneState =
      SceneStateBrowserAgent::FromBrowser(_browser)->GetSceneState();
  _uiBlocker = std::make_unique<ScopedUIBlocker>(sceneState);
  self.signinPromoViewState = ios::SigninPromoViewState::UsedAtLeastOnce;
  self.signinInProgress = YES;
  _signinCoordinator = [SigninCoordinator
      addAccountCoordinatorWithBaseViewController:_baseViewController
                                          browser:_browser
                                      accessPoint:_accessPoint];
  __weak __typeof(self) weakSelf = self;
  _signinCoordinator.signinCompletion =
      ^(SigninCoordinatorResult result, SigninCompletionInfo* info) {
        [weakSelf addAccountDoneWithResult:result info:info];
      };
  [_signinCoordinator start];
}

- (void)addAccountDoneWithResult:(SigninCoordinatorResult)result
                            info:(SigninCompletionInfo*)info {
  DCHECK(_signinCoordinator) << base::SysNSStringToUTF8([self description]);
  _signinCoordinator = nil;
  switch (result) {
    case SigninCoordinatorResultSuccess:
      self.identity = info.identity;
      [self startSignInOnlyFlow];
      break;
    case SigninCoordinatorResultDisabled:
    case SigninCoordinatorResultInterrupted:
    case SigninCoordinatorResultCanceledByUser:
      _uiBlocker.reset();
      self.signinInProgress = NO;
      break;
  }
}

// Changes the promo view state, and records the metrics.
- (void)signinPromoViewIsRemoved {
  DCHECK_NE(ios::SigninPromoViewState::Invalid, self.signinPromoViewState)
      << base::SysNSStringToUTF8([self description]);
  BOOL wasNeverVisible =
      self.signinPromoViewState == ios::SigninPromoViewState::NeverVisible;
  BOOL wasUnused =
      self.signinPromoViewState == ios::SigninPromoViewState::Unused;
  self.signinPromoViewState = ios::SigninPromoViewState::Invalid;
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
  return _dataTypeToWaitForInitialSync != syncer::ModelType::UNSPECIFIED;
}

// If initial sync is needed before the sign-in completes, set the
// `signinInProgress` according to the initial sync state.
- (void)updateSignInProgressWithSyncState {
  if (![self shouldWaitForInitialSync]) {
    return;
  }
  self.signinInProgress = [self isPerformingInitialSync];
}

// Whether the initial sync of the ModelType given by
// `_dataTypeToWaitForInitialSync` is in progress.
- (BOOL)isPerformingInitialSync {
  CHECK(_dataTypeToWaitForInitialSync != syncer::ModelType::UNSPECIFIED);
  return _syncService->GetTypesWithPendingDownloadForInitialSync().Has(
      _dataTypeToWaitForInitialSync);
}

#pragma mark - ChromeAccountManagerServiceObserver

- (void)identityListChanged {
  id<SystemIdentity> currentIdentity = self.identity;
  id<SystemIdentity> defaultIdentity = [self defaultIdentity];
  if (![currentIdentity isEqual:defaultIdentity]) {
    // Don't update the the sign-in promo if the sign-in is in progress,
    // to avoid flashes of the promo.
    self.identity = defaultIdentity;
    [self sendConsumerNotificationWithIdentityChanged:YES];
  }
}

- (void)identityUpdated:(id<SystemIdentity>)identity {
  [self sendConsumerNotificationWithIdentityChanged:NO];
}

#pragma mark - SigninPromoViewDelegate

- (void)signinPromoViewDidTapSigninWithNewAccount:
    (SigninPromoView*)signinPromoView {
  DCHECK(!self.identity) << base::SysNSStringToUTF8([self description]);
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
  signin_metrics::PromoAction promo_action =
      signin_metrics::PromoAction::PROMO_ACTION_NEW_ACCOUNT_NO_EXISTING_ACCOUNT;
  signin_metrics::RecordSigninUserActionForAccessPoint(self.accessPoint);
  if (self.signInOnly) {
    [self startAddAccountForSignInOnly];
    return;
  }
  [self showSigninWithIdentity:nil promoAction:promo_action];
}

- (void)signinPromoViewDidTapSigninWithDefaultAccount:
    (SigninPromoView*)signinPromoView {
  DCHECK(self.identity) << base::SysNSStringToUTF8([self description]);
  DCHECK(self.signinPromoViewVisible)
      << base::SysNSStringToUTF8([self description]);
  DCHECK(!self.invalidClosedOrNeverVisible)
      << base::SysNSStringToUTF8([self description]);
  [self sendImpressionsTillSigninButtonsHistogram];
  if (self.signInOnly) {
    [self primaryActionForSignInOnly];
    return;
  }
  [self showSigninWithIdentity:self.identity
                   promoAction:signin_metrics::PromoAction::
                                   PROMO_ACTION_WITH_DEFAULT];
}

- (void)signinPromoViewDidTapSigninWithOtherAccount:
    (SigninPromoView*)signinPromoView {
  DCHECK(self.identity) << base::SysNSStringToUTF8([self description]);
  DCHECK(self.signinPromoViewVisible)
      << base::SysNSStringToUTF8([self description]);
  DCHECK(!self.invalidClosedOrNeverVisible)
      << base::SysNSStringToUTF8([self description]);
  [self sendImpressionsTillSigninButtonsHistogram];
  signin_metrics::RecordSigninUserActionForAccessPoint(self.accessPoint);
  if (self.signInOnly) {
    [self secondaryActionForSignInOnly];
    return;
  }
  [self showSigninWithIdentity:nil
                   promoAction:signin_metrics::PromoAction::
                                   PROMO_ACTION_NOT_DEFAULT];
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
  self.signinPromoViewState = ios::SigninPromoViewState::Closed;
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

#pragma mark - IdentityChooserCoordinatorDelegate

- (void)identityChooserCoordinatorDidClose:
    (IdentityChooserCoordinator*)coordinator {
  // `_identityChooserCoordinator.delegate` was set to nil before calling this
  // method since `identityChooserCoordinatorDidTapOnAddAccount:` or
  // `identityChooserCoordinator:didSelectIdentity:` have been called before.
  NOTREACHED() << base::SysNSStringToUTF8([self description]);
}

- (void)identityChooserCoordinatorDidTapOnAddAccount:
    (IdentityChooserCoordinator*)coordinator {
  DCHECK_EQ(coordinator, _identityChooserCoordinator)
      << base::SysNSStringToUTF8([self description]);
  _identityChooserCoordinator.delegate = nil;
  [_identityChooserCoordinator stop];
  _identityChooserCoordinator = nil;
  [self startAddAccountForSignInOnly];
}

- (void)identityChooserCoordinator:(IdentityChooserCoordinator*)coordinator
                 didSelectIdentity:(id<SystemIdentity>)identity {
  DCHECK_EQ(coordinator, _identityChooserCoordinator)
      << base::SysNSStringToUTF8([self description]);
  _identityChooserCoordinator.delegate = nil;
  [_identityChooserCoordinator stop];
  _identityChooserCoordinator = nil;
  if (!identity) {
    self.signinInProgress = NO;
    _uiBlocker.reset();
    return;
  }
  self.identity = identity;
  [_identityChooserCoordinator stop];
  _identityChooserCoordinator = nil;
  [self startSignInOnlyFlow];
}

#pragma mark - SyncObserverModelBridge

// If additional data sync is needed during sign-in, update `signinInProgress`
// to match the sync progress state when the sync state changes. This is needed
// especially when the sign-in is undone during initial sync and
// `onSyncConfigurationCompleted` is not called.
- (void)onSyncStateChanged {
  [self updateSignInProgressWithSyncState];
}

// If additional data sync is needed during sign-in, set `signinInProgress`
// to NO when the initial sync finishes for the data type.
- (void)onSyncConfigurationCompleted {
  if (![self shouldWaitForInitialSync] || [self isPerformingInitialSync]) {
    return;
  }
  // Handle sign-in completion.
  self.signinInProgress = NO;
  if ([self.consumer respondsToSelector:@selector(signinDidFinish)]) {
    [self.consumer signinDidFinish];
  }
}

#pragma mark - NSObject

- (NSString*)description {
  return [NSString
      stringWithFormat:@"<%@: %p, identity: %p, signinPromoViewState: %d, "
                       @"signinInProgress: %d, accessPoint: %d, "
                       @"signinPromoViewVisible: %d, invalidOrClosed %d>",
                       self.class.description, self, self.identity,
                       self.signinPromoViewState, self.signinInProgress,
                       self.accessPoint, self.signinPromoViewVisible,
                       self.invalidOrClosed];
}

@end
