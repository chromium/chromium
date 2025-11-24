// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/signin_promo_view_mediator.h"

#import <memory>

#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "components/signin/public/identity_manager/tribool.h"
#import "components/sync/service/sync_user_settings.h"
#import "ios/chrome/browser/authentication/ui_bundled/account_settings_presenter.h"
#import "ios/chrome/browser/authentication/ui_bundled/cells/signin_promo_view_configurator.h"
#import "ios/chrome/browser/authentication/ui_bundled/cells/signin_promo_view_consumer.h"
#import "ios/chrome/browser/authentication/ui_bundled/change_profile_continuation_provider.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_utils.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_presenter.h"
#import "ios/chrome/browser/discover_feed/model/feed_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/avatar_provider.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/sync/model/sync_observer_bridge.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Number of times the sign-in promo should be displayed until it is
// automatically dismissed.
constexpr int kAutomaticSigninPromoViewDismissCount = 20;

// Number of times the top-of-feed sync promo is shown before being
// automatically dismissed.
constexpr int kFeedSyncPromoAutodismissCount = 6;

// User defaults key to get the last logged impression of the top-of-feed promo.
NSString* const kLastSigninImpressionTopOfFeedKey =
    @"last_signin_impression_top_of_feed";
// The time representing a session to increment the impression count of the
// top-of-feed promo, in seconds.
constexpr int kTopOfFeedSessionTimeInterval = 60 * 30;

// Returns true if the sign-in promo is supported for `access_point`.
bool IsSupportedAccessPoint(signin_metrics::AccessPoint access_point) {
  switch (access_point) {
    case signin_metrics::AccessPoint::kBookmarkManager:
    case signin_metrics::AccessPoint::kRecentTabs:
    case signin_metrics::AccessPoint::kNtpFeedTopPromo:
    case signin_metrics::AccessPoint::kReadingList:
      return true;
    case signin_metrics::AccessPoint::kSettings:
    case signin_metrics::AccessPoint::kSettingsYourSavedInfo:
    // TODO(crbug.com/40280655): Pass kTabSwitcher and not recent
    // tabs in the tab switcher promo.
    case signin_metrics::AccessPoint::kTabSwitcher:
    case signin_metrics::AccessPoint::kPostDeviceRestoreSigninPromo:
    case signin_metrics::AccessPoint::kNtpFeedCardMenuPromo:
    case signin_metrics::AccessPoint::kNtpFeedBottomPromo:
    case signin_metrics::AccessPoint::kStartPage:
    case signin_metrics::AccessPoint::kNtpLink:
    case signin_metrics::AccessPoint::kMenu:
    case signin_metrics::AccessPoint::kSupervisedUser:
    case signin_metrics::AccessPoint::kExtensionInstallBubble:
    case signin_metrics::AccessPoint::kExtensions:
    case signin_metrics::AccessPoint::kBookmarkBubble:
    case signin_metrics::AccessPoint::kAvatarBubbleSignIn:
    case signin_metrics::AccessPoint::kUserManager:
    case signin_metrics::AccessPoint::kDevicesPage:
    case signin_metrics::AccessPoint::kFullscreenSigninPromo:
    case signin_metrics::AccessPoint::kUnknown:
    case signin_metrics::AccessPoint::kPasswordBubble:
    case signin_metrics::AccessPoint::kAutofillDropdown:
    case signin_metrics::AccessPoint::kResigninInfobar:
    case signin_metrics::AccessPoint::kMachineLogon:
    case signin_metrics::AccessPoint::kGoogleServicesSettings:
    case signin_metrics::AccessPoint::kSyncErrorCard:
    case signin_metrics::AccessPoint::kForcedSignin:
    case signin_metrics::AccessPoint::kAccountRenamed:
    case signin_metrics::AccessPoint::kWebSignin:
    case signin_metrics::AccessPoint::kSafetyCheck:
    case signin_metrics::AccessPoint::kKaleidoscope:
    case signin_metrics::AccessPoint::kEnterpriseSignoutCoordinator:
    case signin_metrics::AccessPoint::kSigninInterceptFirstRunExperience:
    case signin_metrics::AccessPoint::kSendTabToSelfPromo:
    case signin_metrics::AccessPoint::kSettingsSyncOffRow:
    case signin_metrics::AccessPoint::kPostDeviceRestoreBackgroundSignin:
    case signin_metrics::AccessPoint::kNtpSignedOutIcon:
    case signin_metrics::AccessPoint::kDesktopSigninManager:
    case signin_metrics::AccessPoint::kForYouFre:
    case signin_metrics::AccessPoint::kCreatorFeedFollow:
    case signin_metrics::AccessPoint::kReauthInfoBar:
    case signin_metrics::AccessPoint::kAccountConsistencyService:
    case signin_metrics::AccessPoint::kSearchCompanion:
    case signin_metrics::AccessPoint::kSetUpList:
    case signin_metrics::AccessPoint::kSaveToDriveIos:
    case signin_metrics::AccessPoint::kSaveToPhotosIos:
    case signin_metrics::AccessPoint::kChromeSigninInterceptBubble:
    case signin_metrics::AccessPoint::kRestorePrimaryAccountOnProfileLoad:
    case signin_metrics::AccessPoint::kTabOrganization:
    case signin_metrics::AccessPoint::kTipsNotification:
    case signin_metrics::AccessPoint::kNotificationsOptInScreenContentToggle:
    case signin_metrics::AccessPoint::kSigninChoiceRemembered:
    case signin_metrics::AccessPoint::kProfileMenuSignoutConfirmationPrompt:
    case signin_metrics::AccessPoint::kSettingsSignoutConfirmationPrompt:
    case signin_metrics::AccessPoint::kNtpIdentityDisc:
    case signin_metrics::AccessPoint::kOidcRedirectionInterception:
    case signin_metrics::AccessPoint::kWebauthnModalDialog:
    case signin_metrics::AccessPoint::kAvatarBubbleSignInWithSyncPromo:
    case signin_metrics::AccessPoint::kAccountMenuSwitchAccount:
    case signin_metrics::AccessPoint::kAccountMenuSwitchAccountFailed:
    case signin_metrics::AccessPoint::kProductSpecifications:
    case signin_metrics::AccessPoint::kAddressBubble:
    case signin_metrics::AccessPoint::kCctAccountMismatchNotification:
    case signin_metrics::AccessPoint::kDriveFilePickerIos:
    case signin_metrics::AccessPoint::kCollaborationShareTabGroup:
    case signin_metrics::AccessPoint::kGlicLaunchButton:
    case signin_metrics::AccessPoint::kHistoryPage:
    case signin_metrics::AccessPoint::kCollaborationJoinTabGroup:
    case signin_metrics::AccessPoint::kHistorySyncOptinExpansionPillOnStartup:
    case signin_metrics::AccessPoint::kWidget:
    case signin_metrics::AccessPoint::kCollaborationLeaveOrDeleteTabGroup:
    case signin_metrics::AccessPoint::kHistorySyncEducationalTip:
    case signin_metrics::AccessPoint::kManagedProfileAutoSigninIos:
    case signin_metrics::AccessPoint::kNonModalSigninPasswordPromo:
    case signin_metrics::AccessPoint::kNonModalSigninBookmarkPromo:
    case signin_metrics::AccessPoint::kUserManagerWithPrefilledEmail:
    case signin_metrics::AccessPoint::kEnterpriseManagementDisclaimerAtStartup:
    case signin_metrics::AccessPoint::
        kEnterpriseManagementDisclaimerAfterBrowserFocus:
    case signin_metrics::AccessPoint::
        kEnterpriseManagementDisclaimerAfterSignin:
    case signin_metrics::AccessPoint::kNtpFeaturePromo:
    case signin_metrics::AccessPoint::kEnterpriseDialogAfterSigninInterception:
      return false;
  }
}

// Records in histogram, the number of times the sign-in promo is displayed
// before the sign-in button is pressed.
void RecordImpressionsTilSigninButtonsHistogramForAccessPoint(
    signin_metrics::AccessPoint access_point,
    int displayed_count) {
  switch (access_point) {
    case signin_metrics::AccessPoint::kBookmarkManager:
      base::UmaHistogramCounts100(
          "MobileSignInPromo.BookmarkManager.ImpressionsTilSigninButtons",
          displayed_count);
      break;
    case signin_metrics::AccessPoint::kNtpFeedTopPromo:
      base::UmaHistogramCounts100(
          "MobileSignInPromo.NTPFeedTop.ImpressionsTilSigninButtons",
          displayed_count);
      break;
    case signin_metrics::AccessPoint::kReadingList:
      base::UmaHistogramCounts100(
          "MobileSignInPromo.ReadingList.ImpressionsTilSigninButtons",
          displayed_count);
      break;
    case signin_metrics::AccessPoint::kEnterpriseSignoutCoordinator:
    case signin_metrics::AccessPoint::kSettings:
    case signin_metrics::AccessPoint::kSettingsYourSavedInfo:
    case signin_metrics::AccessPoint::kRecentTabs:
    case signin_metrics::AccessPoint::kTabSwitcher:
    case signin_metrics::AccessPoint::kStartPage:
    case signin_metrics::AccessPoint::kNtpLink:
    case signin_metrics::AccessPoint::kMenu:
    case signin_metrics::AccessPoint::kSupervisedUser:
    case signin_metrics::AccessPoint::kExtensionInstallBubble:
    case signin_metrics::AccessPoint::kExtensions:
    case signin_metrics::AccessPoint::kBookmarkBubble:
    case signin_metrics::AccessPoint::kAvatarBubbleSignIn:
    case signin_metrics::AccessPoint::kUserManager:
    case signin_metrics::AccessPoint::kDevicesPage:
    case signin_metrics::AccessPoint::kFullscreenSigninPromo:
    case signin_metrics::AccessPoint::kUnknown:
    case signin_metrics::AccessPoint::kPasswordBubble:
    case signin_metrics::AccessPoint::kAutofillDropdown:
    case signin_metrics::AccessPoint::kResigninInfobar:
    case signin_metrics::AccessPoint::kMachineLogon:
    case signin_metrics::AccessPoint::kGoogleServicesSettings:
    case signin_metrics::AccessPoint::kSyncErrorCard:
    case signin_metrics::AccessPoint::kForcedSignin:
    case signin_metrics::AccessPoint::kAccountRenamed:
    case signin_metrics::AccessPoint::kWebSignin:
    case signin_metrics::AccessPoint::kSafetyCheck:
    case signin_metrics::AccessPoint::kKaleidoscope:
    case signin_metrics::AccessPoint::kSigninInterceptFirstRunExperience:
    case signin_metrics::AccessPoint::kSendTabToSelfPromo:
    case signin_metrics::AccessPoint::kSettingsSyncOffRow:
    case signin_metrics::AccessPoint::kPostDeviceRestoreSigninPromo:
    case signin_metrics::AccessPoint::kPostDeviceRestoreBackgroundSignin:
    case signin_metrics::AccessPoint::kNtpSignedOutIcon:
    case signin_metrics::AccessPoint::kNtpFeedCardMenuPromo:
    case signin_metrics::AccessPoint::kNtpFeedBottomPromo:
    case signin_metrics::AccessPoint::kDesktopSigninManager:
    case signin_metrics::AccessPoint::kForYouFre:
    case signin_metrics::AccessPoint::kCreatorFeedFollow:
    case signin_metrics::AccessPoint::kReauthInfoBar:
    case signin_metrics::AccessPoint::kAccountConsistencyService:
    case signin_metrics::AccessPoint::kSearchCompanion:
    case signin_metrics::AccessPoint::kSetUpList:
    case signin_metrics::AccessPoint::kSaveToDriveIos:
    case signin_metrics::AccessPoint::kSaveToPhotosIos:
    case signin_metrics::AccessPoint::kChromeSigninInterceptBubble:
    case signin_metrics::AccessPoint::kRestorePrimaryAccountOnProfileLoad:
    case signin_metrics::AccessPoint::kTabOrganization:
    case signin_metrics::AccessPoint::kTipsNotification:
    case signin_metrics::AccessPoint::kNotificationsOptInScreenContentToggle:
    case signin_metrics::AccessPoint::kSigninChoiceRemembered:
    case signin_metrics::AccessPoint::kProfileMenuSignoutConfirmationPrompt:
    case signin_metrics::AccessPoint::kSettingsSignoutConfirmationPrompt:
    case signin_metrics::AccessPoint::kNtpIdentityDisc:
    case signin_metrics::AccessPoint::kOidcRedirectionInterception:
    case signin_metrics::AccessPoint::kWebauthnModalDialog:
    case signin_metrics::AccessPoint::kAvatarBubbleSignInWithSyncPromo:
    case signin_metrics::AccessPoint::kAccountMenuSwitchAccount:
    case signin_metrics::AccessPoint::kAccountMenuSwitchAccountFailed:
    case signin_metrics::AccessPoint::kProductSpecifications:
    case signin_metrics::AccessPoint::kAddressBubble:
    case signin_metrics::AccessPoint::kCctAccountMismatchNotification:
    case signin_metrics::AccessPoint::kDriveFilePickerIos:
    case signin_metrics::AccessPoint::kCollaborationShareTabGroup:
    case signin_metrics::AccessPoint::kGlicLaunchButton:
    case signin_metrics::AccessPoint::kHistoryPage:
    case signin_metrics::AccessPoint::kCollaborationJoinTabGroup:
    case signin_metrics::AccessPoint::kHistorySyncOptinExpansionPillOnStartup:
    case signin_metrics::AccessPoint::kWidget:
    case signin_metrics::AccessPoint::kCollaborationLeaveOrDeleteTabGroup:
    case signin_metrics::AccessPoint::kHistorySyncEducationalTip:
    case signin_metrics::AccessPoint::kManagedProfileAutoSigninIos:
    case signin_metrics::AccessPoint::kNonModalSigninPasswordPromo:
    case signin_metrics::AccessPoint::kNonModalSigninBookmarkPromo:
    case signin_metrics::AccessPoint::kUserManagerWithPrefilledEmail:
    case signin_metrics::AccessPoint::kEnterpriseManagementDisclaimerAtStartup:
    case signin_metrics::AccessPoint::
        kEnterpriseManagementDisclaimerAfterBrowserFocus:
    case signin_metrics::AccessPoint::
        kEnterpriseManagementDisclaimerAfterSignin:
    case signin_metrics::AccessPoint::kNtpFeaturePromo:
    case signin_metrics::AccessPoint::kEnterpriseDialogAfterSigninInterception:
      NOTREACHED() << "Unexpected value for access point "
                   << static_cast<int>(access_point);
  }
}

// Records in histogram, the number of times the sign-in promo is displayed
// before the close button is pressed.
void RecordImpressionsTilXButtonHistogramForAccessPoint(
    signin_metrics::AccessPoint access_point,
    int displayed_count) {
  switch (access_point) {
    case signin_metrics::AccessPoint::kBookmarkManager:
      base::UmaHistogramCounts100(
          "MobileSignInPromo.BookmarkManager.ImpressionsTilXButton",
          displayed_count);
      break;
    case signin_metrics::AccessPoint::kNtpFeedTopPromo:
      base::UmaHistogramCounts100(
          "MobileSignInPromo.NTPFeedTop.ImpressionsTilXButton",
          displayed_count);
      break;
    case signin_metrics::AccessPoint::kReadingList:
      base::UmaHistogramCounts100(
          "MobileSignInPromo.ReadingList.ImpressionsTilXButton",
          displayed_count);
      break;
    case signin_metrics::AccessPoint::kEnterpriseSignoutCoordinator:
    case signin_metrics::AccessPoint::kSettings:
    case signin_metrics::AccessPoint::kSettingsYourSavedInfo:
    case signin_metrics::AccessPoint::kRecentTabs:
    case signin_metrics::AccessPoint::kTabSwitcher:
    case signin_metrics::AccessPoint::kStartPage:
    case signin_metrics::AccessPoint::kNtpLink:
    case signin_metrics::AccessPoint::kMenu:
    case signin_metrics::AccessPoint::kSupervisedUser:
    case signin_metrics::AccessPoint::kExtensionInstallBubble:
    case signin_metrics::AccessPoint::kExtensions:
    case signin_metrics::AccessPoint::kBookmarkBubble:
    case signin_metrics::AccessPoint::kAvatarBubbleSignIn:
    case signin_metrics::AccessPoint::kUserManager:
    case signin_metrics::AccessPoint::kDevicesPage:
    case signin_metrics::AccessPoint::kFullscreenSigninPromo:
    case signin_metrics::AccessPoint::kUnknown:
    case signin_metrics::AccessPoint::kPasswordBubble:
    case signin_metrics::AccessPoint::kAutofillDropdown:
    case signin_metrics::AccessPoint::kResigninInfobar:
    case signin_metrics::AccessPoint::kMachineLogon:
    case signin_metrics::AccessPoint::kGoogleServicesSettings:
    case signin_metrics::AccessPoint::kSyncErrorCard:
    case signin_metrics::AccessPoint::kForcedSignin:
    case signin_metrics::AccessPoint::kAccountRenamed:
    case signin_metrics::AccessPoint::kWebSignin:
    case signin_metrics::AccessPoint::kSafetyCheck:
    case signin_metrics::AccessPoint::kKaleidoscope:
    case signin_metrics::AccessPoint::kSigninInterceptFirstRunExperience:
    case signin_metrics::AccessPoint::kSendTabToSelfPromo:
    case signin_metrics::AccessPoint::kSettingsSyncOffRow:
    case signin_metrics::AccessPoint::kPostDeviceRestoreSigninPromo:
    case signin_metrics::AccessPoint::kPostDeviceRestoreBackgroundSignin:
    case signin_metrics::AccessPoint::kNtpSignedOutIcon:
    case signin_metrics::AccessPoint::kNtpFeedCardMenuPromo:
    case signin_metrics::AccessPoint::kNtpFeedBottomPromo:
    case signin_metrics::AccessPoint::kDesktopSigninManager:
    case signin_metrics::AccessPoint::kForYouFre:
    case signin_metrics::AccessPoint::kCreatorFeedFollow:
    case signin_metrics::AccessPoint::kReauthInfoBar:
    case signin_metrics::AccessPoint::kAccountConsistencyService:
    case signin_metrics::AccessPoint::kSearchCompanion:
    case signin_metrics::AccessPoint::kSetUpList:
    case signin_metrics::AccessPoint::kSaveToDriveIos:
    case signin_metrics::AccessPoint::kSaveToPhotosIos:
    case signin_metrics::AccessPoint::kChromeSigninInterceptBubble:
    case signin_metrics::AccessPoint::kRestorePrimaryAccountOnProfileLoad:
    case signin_metrics::AccessPoint::kTabOrganization:
    case signin_metrics::AccessPoint::kTipsNotification:
    case signin_metrics::AccessPoint::kNotificationsOptInScreenContentToggle:
    case signin_metrics::AccessPoint::kSigninChoiceRemembered:
    case signin_metrics::AccessPoint::kProfileMenuSignoutConfirmationPrompt:
    case signin_metrics::AccessPoint::kSettingsSignoutConfirmationPrompt:
    case signin_metrics::AccessPoint::kNtpIdentityDisc:
    case signin_metrics::AccessPoint::kOidcRedirectionInterception:
    case signin_metrics::AccessPoint::kWebauthnModalDialog:
    case signin_metrics::AccessPoint::kAvatarBubbleSignInWithSyncPromo:
    case signin_metrics::AccessPoint::kAccountMenuSwitchAccount:
    case signin_metrics::AccessPoint::kAccountMenuSwitchAccountFailed:
    case signin_metrics::AccessPoint::kProductSpecifications:
    case signin_metrics::AccessPoint::kAddressBubble:
    case signin_metrics::AccessPoint::kCctAccountMismatchNotification:
    case signin_metrics::AccessPoint::kDriveFilePickerIos:
    case signin_metrics::AccessPoint::kCollaborationShareTabGroup:
    case signin_metrics::AccessPoint::kGlicLaunchButton:
    case signin_metrics::AccessPoint::kHistoryPage:
    case signin_metrics::AccessPoint::kCollaborationJoinTabGroup:
    case signin_metrics::AccessPoint::kHistorySyncOptinExpansionPillOnStartup:
    case signin_metrics::AccessPoint::kWidget:
    case signin_metrics::AccessPoint::kCollaborationLeaveOrDeleteTabGroup:
    case signin_metrics::AccessPoint::kHistorySyncEducationalTip:
    case signin_metrics::AccessPoint::kManagedProfileAutoSigninIos:
    case signin_metrics::AccessPoint::kNonModalSigninPasswordPromo:
    case signin_metrics::AccessPoint::kNonModalSigninBookmarkPromo:
    case signin_metrics::AccessPoint::kUserManagerWithPrefilledEmail:
    case signin_metrics::AccessPoint::kEnterpriseManagementDisclaimerAtStartup:
    case signin_metrics::AccessPoint::
        kEnterpriseManagementDisclaimerAfterBrowserFocus:
    case signin_metrics::AccessPoint::
        kEnterpriseManagementDisclaimerAfterSignin:
    case signin_metrics::AccessPoint::kNtpFeaturePromo:
    case signin_metrics::AccessPoint::kEnterpriseDialogAfterSigninInterception:
      NOTREACHED() << "Unexpected value for access point "
                   << static_cast<int>(access_point);
  }
}

// Returns the DisplayedCount preference key string for `access_point`.
const char* DisplayedCountPreferenceKey(
    signin_metrics::AccessPoint access_point) {
  switch (access_point) {
    case signin_metrics::AccessPoint::kBookmarkManager:
      return prefs::kIosBookmarkSigninPromoDisplayedCount;
    case signin_metrics::AccessPoint::kNtpFeedTopPromo:
      return prefs::kIosNtpFeedTopSigninPromoDisplayedCount;
    case signin_metrics::AccessPoint::kReadingList:
      return prefs::kIosReadingListSigninPromoDisplayedCount;
    case signin_metrics::AccessPoint::kSettings:
    case signin_metrics::AccessPoint::kSettingsYourSavedInfo:
    case signin_metrics::AccessPoint::kRecentTabs:
    case signin_metrics::AccessPoint::kTabSwitcher:
    case signin_metrics::AccessPoint::kStartPage:
    case signin_metrics::AccessPoint::kNtpLink:
    case signin_metrics::AccessPoint::kMenu:
    case signin_metrics::AccessPoint::kSupervisedUser:
    case signin_metrics::AccessPoint::kExtensionInstallBubble:
    case signin_metrics::AccessPoint::kExtensions:
    case signin_metrics::AccessPoint::kBookmarkBubble:
    case signin_metrics::AccessPoint::kAvatarBubbleSignIn:
    case signin_metrics::AccessPoint::kUserManager:
    case signin_metrics::AccessPoint::kDevicesPage:
    case signin_metrics::AccessPoint::kFullscreenSigninPromo:
    case signin_metrics::AccessPoint::kUnknown:
    case signin_metrics::AccessPoint::kPasswordBubble:
    case signin_metrics::AccessPoint::kAutofillDropdown:
    case signin_metrics::AccessPoint::kResigninInfobar:
    case signin_metrics::AccessPoint::kMachineLogon:
    case signin_metrics::AccessPoint::kGoogleServicesSettings:
    case signin_metrics::AccessPoint::kSyncErrorCard:
    case signin_metrics::AccessPoint::kForcedSignin:
    case signin_metrics::AccessPoint::kAccountRenamed:
    case signin_metrics::AccessPoint::kWebSignin:
    case signin_metrics::AccessPoint::kSafetyCheck:
    case signin_metrics::AccessPoint::kKaleidoscope:
    case signin_metrics::AccessPoint::kEnterpriseSignoutCoordinator:
    case signin_metrics::AccessPoint::kSigninInterceptFirstRunExperience:
    case signin_metrics::AccessPoint::kSendTabToSelfPromo:
    case signin_metrics::AccessPoint::kSettingsSyncOffRow:
    case signin_metrics::AccessPoint::kPostDeviceRestoreSigninPromo:
    case signin_metrics::AccessPoint::kPostDeviceRestoreBackgroundSignin:
    case signin_metrics::AccessPoint::kNtpSignedOutIcon:
    case signin_metrics::AccessPoint::kNtpFeedCardMenuPromo:
    case signin_metrics::AccessPoint::kNtpFeedBottomPromo:
    case signin_metrics::AccessPoint::kDesktopSigninManager:
    case signin_metrics::AccessPoint::kForYouFre:
    case signin_metrics::AccessPoint::kCreatorFeedFollow:
    case signin_metrics::AccessPoint::kReauthInfoBar:
    case signin_metrics::AccessPoint::kAccountConsistencyService:
    case signin_metrics::AccessPoint::kSearchCompanion:
    case signin_metrics::AccessPoint::kSetUpList:
    case signin_metrics::AccessPoint::kSaveToDriveIos:
    case signin_metrics::AccessPoint::kSaveToPhotosIos:
    case signin_metrics::AccessPoint::kChromeSigninInterceptBubble:
    case signin_metrics::AccessPoint::kRestorePrimaryAccountOnProfileLoad:
    case signin_metrics::AccessPoint::kTabOrganization:
    case signin_metrics::AccessPoint::kTipsNotification:
    case signin_metrics::AccessPoint::kNotificationsOptInScreenContentToggle:
    case signin_metrics::AccessPoint::kSigninChoiceRemembered:
    case signin_metrics::AccessPoint::kProfileMenuSignoutConfirmationPrompt:
    case signin_metrics::AccessPoint::kSettingsSignoutConfirmationPrompt:
    case signin_metrics::AccessPoint::kNtpIdentityDisc:
    case signin_metrics::AccessPoint::kOidcRedirectionInterception:
    case signin_metrics::AccessPoint::kWebauthnModalDialog:
    case signin_metrics::AccessPoint::kAvatarBubbleSignInWithSyncPromo:
    case signin_metrics::AccessPoint::kAccountMenuSwitchAccount:
    case signin_metrics::AccessPoint::kAccountMenuSwitchAccountFailed:
    case signin_metrics::AccessPoint::kProductSpecifications:
    case signin_metrics::AccessPoint::kAddressBubble:
    case signin_metrics::AccessPoint::kCctAccountMismatchNotification:
    case signin_metrics::AccessPoint::kDriveFilePickerIos:
    case signin_metrics::AccessPoint::kCollaborationShareTabGroup:
    case signin_metrics::AccessPoint::kGlicLaunchButton:
    case signin_metrics::AccessPoint::kHistoryPage:
    case signin_metrics::AccessPoint::kCollaborationJoinTabGroup:
    case signin_metrics::AccessPoint::kHistorySyncOptinExpansionPillOnStartup:
    case signin_metrics::AccessPoint::kWidget:
    case signin_metrics::AccessPoint::kCollaborationLeaveOrDeleteTabGroup:
    case signin_metrics::AccessPoint::kHistorySyncEducationalTip:
    case signin_metrics::AccessPoint::kManagedProfileAutoSigninIos:
    case signin_metrics::AccessPoint::kNonModalSigninPasswordPromo:
    case signin_metrics::AccessPoint::kNonModalSigninBookmarkPromo:
    case signin_metrics::AccessPoint::kUserManagerWithPrefilledEmail:
    case signin_metrics::AccessPoint::kEnterpriseManagementDisclaimerAtStartup:
    case signin_metrics::AccessPoint::
        kEnterpriseManagementDisclaimerAfterBrowserFocus:
    case signin_metrics::AccessPoint::
        kEnterpriseManagementDisclaimerAfterSignin:
    case signin_metrics::AccessPoint::kNtpFeaturePromo:
    case signin_metrics::AccessPoint::kEnterpriseDialogAfterSigninInterception:
      return nullptr;
  }
}

// Returns AlreadySeen preference key string for `access_point`.
const char* AlreadySeenSigninViewPreferenceKey(
    signin_metrics::AccessPoint access_point) {
  switch (access_point) {
    case signin_metrics::AccessPoint::kBookmarkManager:
      return prefs::kIosBookmarkPromoAlreadySeen;
    case signin_metrics::AccessPoint::kNtpFeedTopPromo:
      return prefs::kIosNtpFeedTopPromoAlreadySeen;
    case signin_metrics::AccessPoint::kReadingList:
      return prefs::kIosReadingListPromoAlreadySeen;
    case signin_metrics::AccessPoint::kSettings:
    case signin_metrics::AccessPoint::kSettingsYourSavedInfo:
    case signin_metrics::AccessPoint::kRecentTabs:
    case signin_metrics::AccessPoint::kTabSwitcher:
    case signin_metrics::AccessPoint::kStartPage:
    case signin_metrics::AccessPoint::kNtpLink:
    case signin_metrics::AccessPoint::kMenu:
    case signin_metrics::AccessPoint::kSupervisedUser:
    case signin_metrics::AccessPoint::kExtensionInstallBubble:
    case signin_metrics::AccessPoint::kExtensions:
    case signin_metrics::AccessPoint::kBookmarkBubble:
    case signin_metrics::AccessPoint::kAvatarBubbleSignIn:
    case signin_metrics::AccessPoint::kUserManager:
    case signin_metrics::AccessPoint::kDevicesPage:
    case signin_metrics::AccessPoint::kFullscreenSigninPromo:
    case signin_metrics::AccessPoint::kUnknown:
    case signin_metrics::AccessPoint::kPasswordBubble:
    case signin_metrics::AccessPoint::kAutofillDropdown:
    case signin_metrics::AccessPoint::kResigninInfobar:
    case signin_metrics::AccessPoint::kMachineLogon:
    case signin_metrics::AccessPoint::kGoogleServicesSettings:
    case signin_metrics::AccessPoint::kSyncErrorCard:
    case signin_metrics::AccessPoint::kForcedSignin:
    case signin_metrics::AccessPoint::kAccountRenamed:
    case signin_metrics::AccessPoint::kWebSignin:
    case signin_metrics::AccessPoint::kSafetyCheck:
    case signin_metrics::AccessPoint::kKaleidoscope:
    case signin_metrics::AccessPoint::kEnterpriseSignoutCoordinator:
    case signin_metrics::AccessPoint::kSigninInterceptFirstRunExperience:
    case signin_metrics::AccessPoint::kSendTabToSelfPromo:
    case signin_metrics::AccessPoint::kSettingsSyncOffRow:
    case signin_metrics::AccessPoint::kPostDeviceRestoreSigninPromo:
    case signin_metrics::AccessPoint::kPostDeviceRestoreBackgroundSignin:
    case signin_metrics::AccessPoint::kNtpSignedOutIcon:
    case signin_metrics::AccessPoint::kNtpFeedCardMenuPromo:
    case signin_metrics::AccessPoint::kNtpFeedBottomPromo:
    case signin_metrics::AccessPoint::kDesktopSigninManager:
    case signin_metrics::AccessPoint::kForYouFre:
    case signin_metrics::AccessPoint::kCreatorFeedFollow:
    case signin_metrics::AccessPoint::kReauthInfoBar:
    case signin_metrics::AccessPoint::kAccountConsistencyService:
    case signin_metrics::AccessPoint::kSearchCompanion:
    case signin_metrics::AccessPoint::kSetUpList:
    case signin_metrics::AccessPoint::kSaveToDriveIos:
    case signin_metrics::AccessPoint::kSaveToPhotosIos:
    case signin_metrics::AccessPoint::kChromeSigninInterceptBubble:
    case signin_metrics::AccessPoint::kRestorePrimaryAccountOnProfileLoad:
    case signin_metrics::AccessPoint::kTabOrganization:
    case signin_metrics::AccessPoint::kTipsNotification:
    case signin_metrics::AccessPoint::kNotificationsOptInScreenContentToggle:
    case signin_metrics::AccessPoint::kSigninChoiceRemembered:
    case signin_metrics::AccessPoint::kProfileMenuSignoutConfirmationPrompt:
    case signin_metrics::AccessPoint::kSettingsSignoutConfirmationPrompt:
    case signin_metrics::AccessPoint::kNtpIdentityDisc:
    case signin_metrics::AccessPoint::kOidcRedirectionInterception:
    case signin_metrics::AccessPoint::kWebauthnModalDialog:
    case signin_metrics::AccessPoint::kAvatarBubbleSignInWithSyncPromo:
    case signin_metrics::AccessPoint::kAccountMenuSwitchAccount:
    case signin_metrics::AccessPoint::kAccountMenuSwitchAccountFailed:
    case signin_metrics::AccessPoint::kProductSpecifications:
    case signin_metrics::AccessPoint::kAddressBubble:
    case signin_metrics::AccessPoint::kCctAccountMismatchNotification:
    case signin_metrics::AccessPoint::kDriveFilePickerIos:
    case signin_metrics::AccessPoint::kCollaborationShareTabGroup:
    case signin_metrics::AccessPoint::kGlicLaunchButton:
    case signin_metrics::AccessPoint::kHistoryPage:
    case signin_metrics::AccessPoint::kCollaborationJoinTabGroup:
    case signin_metrics::AccessPoint::kHistorySyncOptinExpansionPillOnStartup:
    case signin_metrics::AccessPoint::kWidget:
    case signin_metrics::AccessPoint::kCollaborationLeaveOrDeleteTabGroup:
    case signin_metrics::AccessPoint::kHistorySyncEducationalTip:
    case signin_metrics::AccessPoint::kManagedProfileAutoSigninIos:
    case signin_metrics::AccessPoint::kNonModalSigninPasswordPromo:
    case signin_metrics::AccessPoint::kNonModalSigninBookmarkPromo:
    case signin_metrics::AccessPoint::kUserManagerWithPrefilledEmail:
    case signin_metrics::AccessPoint::kEnterpriseManagementDisclaimerAtStartup:
    case signin_metrics::AccessPoint::
        kEnterpriseManagementDisclaimerAfterBrowserFocus:
    case signin_metrics::AccessPoint::
        kEnterpriseManagementDisclaimerAfterSignin:
    case signin_metrics::AccessPoint::kNtpFeaturePromo:
    case signin_metrics::AccessPoint::kEnterpriseDialogAfterSigninInterception:
      return nullptr;
  }
}

// Returns AlreadySeen preference key string for `access_point` and
// `promo_action`.
const char* AlreadySeenSigninViewPreferenceKey(
    signin_metrics::AccessPoint access_point,
    SigninPromoAction promo_action) {
  const char* pref_key = nullptr;
  switch (promo_action) {
    case SigninPromoAction::kReviewAccountSettings: {
      if (access_point == signin_metrics::AccessPoint::kBookmarkManager) {
        pref_key = prefs::kIosBookmarkSettingsPromoAlreadySeen;
      } else if (access_point == signin_metrics::AccessPoint::kReadingList) {
        pref_key = prefs::kIosReadingListSettingsPromoAlreadySeen;
      }
      break;
    }
    case SigninPromoAction::kSigninSheet:
    case SigninPromoAction::kInstantSignin:
    case SigninPromoAction::kSigninWithNoDefaultIdentity:
      pref_key = AlreadySeenSigninViewPreferenceKey(access_point);
      break;
  }
  return pref_key;
}

// See documentation of displayedIdentity property.
id<SystemIdentity> GetDisplayedIdentity(
    AuthenticationService* authService,
    signin::IdentityManager* identityManager,
    ChromeAccountManagerService* accountManagerService) {
  CHECK(authService);
  CHECK(identityManager);
  CHECK(accountManagerService);

  if (authService->HasPrimaryIdentity(signin::ConsentLevel::kSignin)) {
    return authService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  }

  return signin::GetDefaultIdentityOnDevice(identityManager,
                                            accountManagerService);
}

}  // namespace

@interface SigninPromoViewMediator () <IdentityManagerObserverBridgeDelegate,
                                       SyncObserverModelBridge>

// Redefined to be readwrite. See documentation in the header file.
@property(nonatomic, strong, readwrite) id<SystemIdentity> displayedIdentity;

// YES if the initial sync for a specific data type is in progress. The data
// type is based on `dataTypeToWaitForInitialSync`.
@property(nonatomic, assign, readwrite) BOOL initialSyncInProgress;

// Presenter which can show the signed-in account settings UI.
@property(nonatomic, weak, readonly) id<AccountSettingsPresenter>
    accountSettingsPresenter;

// User's preferences service.
@property(nonatomic, assign) PrefService* prefService;

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

// Redeclared to be assign.
@property(nonatomic, assign) SigninPromoViewState signinPromoViewState;

@end

@implementation SigninPromoViewMediator {
  // Authentication Service for the user's signed-in state.
  raw_ptr<AuthenticationService> _authService;
  // AccountManager Service used to retrive identities.
  raw_ptr<ChromeAccountManagerService> _accountManagerService;
  // IdentityManager used to retrive identities.
  raw_ptr<signin::IdentityManager> _identityManager;
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserver;
  // Sync service.
  raw_ptr<syncer::SyncService> _syncService;
  // Observer for changes to the sync state.
  std::unique_ptr<SyncObserverBridge> _syncObserverBridge;
  ChangeProfileContinuationProvider _changeProfileContinuationProvider;
  // Presenter which can show signin UI.
  __weak id<SigninPromoViewMediatorDelegate> _delegate;
}

+ (void)registerProfilePrefs:(user_prefs::PrefRegistrySyncable*)registry {
  // Bookmarks
  registry->RegisterBooleanPref(prefs::kIosBookmarkPromoAlreadySeen, false);
  registry->RegisterBooleanPref(prefs::kIosBookmarkSettingsPromoAlreadySeen,
                                false);
  registry->RegisterIntegerPref(prefs::kIosBookmarkSigninPromoDisplayedCount,
                                0);
  // NTP Feed
  registry->RegisterBooleanPref(prefs::kIosNtpFeedTopPromoAlreadySeen, false);
  registry->RegisterIntegerPref(prefs::kIosNtpFeedTopSigninPromoDisplayedCount,
                                0);
  // Reading List
  registry->RegisterBooleanPref(prefs::kIosReadingListPromoAlreadySeen, false);
  registry->RegisterBooleanPref(prefs::kIosReadingListSettingsPromoAlreadySeen,
                                false);
  registry->RegisterIntegerPref(prefs::kIosReadingListSigninPromoDisplayedCount,
                                0);
}

+ (BOOL)shouldDisplaySigninPromoViewWithAccessPoint:
            (signin_metrics::AccessPoint)accessPoint
                                  signinPromoAction:
                                      (SigninPromoAction)signinPromoAction
                              authenticationService:
                                  (AuthenticationService*)authenticationService
                                        prefService:(PrefService*)prefService {
  // Checks if user can't sign in.
  if (!authenticationService->SigninEnabled()) {
    // The user is not allowed to sign-in. The promo cannot be displayed.
    return NO;
  }

  // Always show the feed signin promo if the experimental setting is enabled.
  if (accessPoint == signin_metrics::AccessPoint::kNtpFeedTopPromo &&
      experimental_flags::ShouldForceFeedSigninPromo()) {
    return YES;
  }

  if (signinPromoAction != SigninPromoAction::kReviewAccountSettings) {
    // Checks if the user has exceeded the max impression count.
    const int maxDisplayedCount =
        accessPoint == signin_metrics::AccessPoint::kNtpFeedTopPromo
            ? kFeedSyncPromoAutodismissCount
            : kAutomaticSigninPromoViewDismissCount;
    const char* displayedCountPreferenceKey =
        DisplayedCountPreferenceKey(accessPoint);
    const int displayedCount =
        prefService ? prefService->GetInteger(displayedCountPreferenceKey)
                    : INT_MAX;
    if (displayedCount >= maxDisplayedCount) {
      return NO;
    }
  }

  // For the top-of-feed promo, the user must have engaged with a feed first.
  if (accessPoint == signin_metrics::AccessPoint::kNtpFeedTopPromo &&
      (![[NSUserDefaults standardUserDefaults]
          boolForKey:kEngagedWithFeedKey])) {
    return NO;
  }

  // Checks if user has already acknowledged or dismissed the promo.
  const char* alreadySeenSigninViewPreferenceKey =
      AlreadySeenSigninViewPreferenceKey(accessPoint, signinPromoAction);
  if (alreadySeenSigninViewPreferenceKey && prefService &&
      prefService->GetBoolean(alreadySeenSigninViewPreferenceKey)) {
    return NO;
  }

  // If no conditions are met, show the promo.
  return YES;
}

- (instancetype)
              initWithIdentityManager:(signin::IdentityManager*)identityManager
                accountManagerService:
                    (ChromeAccountManagerService*)accountManagerService
                          authService:(AuthenticationService*)authService
                          prefService:(PrefService*)prefService
                          syncService:(syncer::SyncService*)syncService
                          accessPoint:(signin_metrics::AccessPoint)accessPoint
                             delegate:
                                 (id<SigninPromoViewMediatorDelegate>)delegate
             accountSettingsPresenter:
                 (id<AccountSettingsPresenter>)accountSettingsPresenter
    changeProfileContinuationProvider:(const ChangeProfileContinuationProvider&)
                                          changeProfileContinuationProvider {
  self = [super init];
  if (self) {
    CHECK(changeProfileContinuationProvider);
    CHECK(identityManager);
    CHECK(accountManagerService);
    DCHECK(IsSupportedAccessPoint(accessPoint));
    _identityManager = identityManager;
    _accountManagerService = accountManagerService;
    _changeProfileContinuationProvider = changeProfileContinuationProvider;
    _authService = authService;
    _prefService = prefService;
    _syncService = syncService;
    _accessPoint = accessPoint;
    _signinPromoViewState = SigninPromoViewState::kNeverVisible;
    _signinPromoAction = SigninPromoAction::kInstantSignin;
    _dataTypeToWaitForInitialSync = syncer::DataType::UNSPECIFIED;
    _delegate = delegate;
    _accountSettingsPresenter = accountSettingsPresenter;
    _identityManagerObserver =
        std::make_unique<signin::IdentityManagerObserverBridge>(
            _identityManager, self);
    // Starting the sync state observation enables the sign-in progress to be
    // set to YES even if the user hasn't interacted with the promo. It is
    // intentional to keep UX consistency, given the initial sync cancellation
    // which should end the sign-in progress is tricky to detect.
    _syncObserverBridge =
        std::make_unique<SyncObserverBridge>(self, _syncService);

    id<SystemIdentity> displayedIdentity = GetDisplayedIdentity(
        _authService, _identityManager, _accountManagerService);
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
      AlreadySeenSigninViewPreferenceKey(self.accessPoint,
                                         self.signinPromoAction) != nullptr;
  if (_authService->HasPrimaryIdentity(signin::ConsentLevel::kSignin)) {
    if (!self.displayedIdentity) {
      // TODO(crbug.com/40777223): The default identity should already be known
      // by the mediator. We should not have no identity. This can be reproduced
      // with EGtests with bots. The identity notification might not have
      // received yet. Let's update the promo identity.
      [self onAccountsOnDeviceChanged];
    }
    DCHECK(self.displayedIdentity)
        << base::SysNSStringToUTF8([self description]);
    SigninPromoViewConfigurator* configurator =
        [[SigninPromoViewConfigurator alloc]
            initWithSigninPromoViewMode:
                SigninPromoViewModeSignedInWithPrimaryAccount
                              userEmail:self.displayedIdentity.userEmail
                          userGivenName:self.displayedIdentity.userGivenName
                              userImage:self.displayedIdentityAvatar
                         hasCloseButton:hasCloseButton
                       hasSignInSpinner:self.showSpinner];
    switch (self.signinPromoAction) {
      case SigninPromoAction::kSigninSheet:
      case SigninPromoAction::kInstantSignin:
      case SigninPromoAction::kSigninWithNoDefaultIdentity:
        break;
      case SigninPromoAction::kReviewAccountSettings:
        configurator.primaryButtonTitleOverride =
            l10n_util::GetNSString(IDS_IOS_SIGNIN_PROMO_REVIEW_SETTINGS_BUTTON);
        break;
    }
    return configurator;
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
    case SigninPromoAction::kReviewAccountSettings:
      break;
    case SigninPromoAction::kSigninSheet:
    case SigninPromoAction::kInstantSignin:
    case SigninPromoAction::kSigninWithNoDefaultIdentity:
      configurator.primaryButtonTitleOverride =
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
  signin_metrics::LogSignInOffered(
      self.accessPoint,
      self.displayedIdentity
          ? signin_metrics::PromoAction::PROMO_ACTION_WITH_DEFAULT
          : signin_metrics::PromoAction::
                PROMO_ACTION_NEW_ACCOUNT_NO_EXISTING_ACCOUNT);
  self.signinPromoViewVisible = YES;
  switch (self.signinPromoAction) {
    case SigninPromoAction::kReviewAccountSettings:
      if (self.accessPoint == signin_metrics::AccessPoint::kBookmarkManager) {
        base::RecordAction(base::UserMetricsAction(
            "ReviewAccountSettings_Impression_FromBookmarkManager"));
      } else if (self.accessPoint ==
                 signin_metrics::AccessPoint::kReadingList) {
        base::RecordAction(base::UserMetricsAction(
            "ReviewAccountSettings_Impression_FromReadingListManager"));
      }
      // This action should not contribute to the DisplayedCount pref.
      return;
    case SigninPromoAction::kSigninSheet:
    case SigninPromoAction::kInstantSignin:
    case SigninPromoAction::kSigninWithNoDefaultIdentity:
      break;
  }
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
  if (self.accessPoint == signin_metrics::AccessPoint::kNtpFeedTopPromo) {
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
  // It is possible to interrupt sign-in in progress even if the UI is blocked.
  // This can happen if the user opens an external URL during sign-in.
  [self signinPromoViewIsRemoved];
  self.consumer = nil;
  _accountManagerService = nullptr;
  _identityManager = nullptr;
  _authService = nullptr;
  _syncService = nullptr;
  _identityManagerObserver.reset();
  _syncObserverBridge.reset();
}

// Finishes the sign-in process.
- (void)signinDidCompleteWithResult:(SigninCoordinatorResult)result {
  if (self.signinPromoViewState == SigninPromoViewState::kInvalid) {
    // The mediator owner can remove the view before the sign-in is done.
    return;
  }
  // We can turn on `self.initialSyncInProgress`, if the sign-in is successful.
  // We can't call now GetTypesWithPendingDownloadForInitialSync() related to
  // a post task issue.
  self.initialSyncInProgress = (result == SigninCoordinatorResultSuccess) &&
                               [self shouldWaitForInitialSync];
  CHECK_EQ(SigninPromoViewState::kUsedAtLeastOnce, self.signinPromoViewState,
           base::NotFatalUntil::M144)
      << base::SysNSStringToUTF8([self description]);
  CHECK_NE(self.signinInProgress, signin::Tribool::kFalse,
           base::NotFatalUntil::M146)
      << base::SysNSStringToUTF8([self description]);
  self.signinInProgress = signin::Tribool::kFalse;
}

#pragma mark - Public properties

- (BOOL)isInvalidClosedOrNeverVisible {
  return self.invalidOrClosed ||
         self.signinPromoViewState == SigninPromoViewState::kNeverVisible;
}

- (BOOL)showSpinner {
  // In the unknown case, the sign-in is very probably in progress.
  // It’s quite rare that the sign-in’s view disappear silently. So it seems
  // safe to show the spinner even if the screen is not otherwise frozen.
  return (self.signinInProgress != signin::Tribool::kFalse) ||
         self.initialSyncInProgress;
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
        GetApplicationContext()->GetIdentityAvatarProvider()->GetIdentityAvatar(
            _displayedIdentity, IdentityAvatarSize::SmallSize);
  }
}

// Updates `_signinInProgress` value, and sends a notification the consumer
// to update the sign-in promo, so the progress indicator can be displayed.
- (void)setSigninInProgress:(signin::Tribool)signinInProgress {
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
  if (!displayedCountPreferenceKey) {
    return;
  }
  int displayedCount =
      self.prefService->GetInteger(displayedCountPreferenceKey);
  RecordImpressionsTilSigninButtonsHistogramForAccessPoint(self.accessPoint,
                                                           displayedCount);
}

// Starts sign-in process with the Chrome identity from `identity`.
- (void)showSigninWithIdentity:(id<SystemIdentity>)identity
                     operation:(AuthenticationOperation)operation
                   promoAction:(signin_metrics::PromoAction)promoAction {
  self.signinPromoViewState = SigninPromoViewState::kUsedAtLeastOnce;
  ShowSigninCommand* command = [[ShowSigninCommand alloc]
                      initWithOperation:operation
                               identity:identity
                            accessPoint:self.accessPoint
                            promoAction:promoAction
                             completion:nil
      changeProfileContinuationProvider:_changeProfileContinuationProvider];
  [_delegate showSignin:self command:command];
  // `showSignin` checks that the signin is not in progress. So the value
  // `signinInProgress` must be set after the signinCommand is executed.
  if (@available(iOS 26, *)) {
    // Once iOS 26 is the minimal supported version, signinInProgress can’t be
    // kUnknown anymore, and thus can become a Boolean instead of a tribool.
    self.signinInProgress = signin::Tribool::kTrue;
  } else {
    if (operation != AuthenticationOperation::kInstantSignin) {
      self.signinInProgress = signin::Tribool::kTrue;
    } else {
      // During the add account, the sign-in view may disappear silently until
      // iOS 18. So we can’t be certain that sign in is in progress.
      self.signinInProgress = signin::Tribool::kUnknown;
    }
  }
}

// Shows account settings.
- (void)showAccountSettings {
  DCHECK(self.accountSettingsPresenter);
  self.signinPromoViewState = SigninPromoViewState::kUsedAtLeastOnce;
  [self.accountSettingsPresenter showAccountSettings];
}

// Changes the promo view state, and records the metrics.
- (void)signinPromoViewIsRemoved {
  DCHECK_NE(SigninPromoViewState::kInvalid, self.signinPromoViewState)
      << base::SysNSStringToUTF8([self description]);
  self.signinPromoViewState = SigninPromoViewState::kInvalid;
  self.signinPromoViewVisible = NO;
}

// Whether the sign-in needs to wait for the end of the initial sync to
// complete.
- (BOOL)shouldWaitForInitialSync {
  return self.dataTypeToWaitForInitialSync != syncer::DataType::UNSPECIFIED;
}

#pragma mark -  IdentityManagerObserver

- (void)onAccountsOnDeviceChanged {
  id<SystemIdentity> currentIdentity = self.displayedIdentity;
  id<SystemIdentity> displayedIdentity = GetDisplayedIdentity(
      _authService, _identityManager, _accountManagerService);
  if (![currentIdentity isEqual:displayedIdentity]) {
    // Don't update the the sign-in promo if the sign-in is in progress,
    // to avoid flashes of the promo.
    self.displayedIdentity = displayedIdentity;
    [self sendConsumerNotificationWithIdentityChanged:YES];
  }
}

- (void)onExtendedAccountInfoUpdated:(const AccountInfo&)info {
  [self sendConsumerNotificationWithIdentityChanged:NO];
}

#pragma mark - SigninPromoViewDelegate

- (void)signinPromoViewDidTapSigninWithNewAccount:
    (SigninPromoView*)signinPromoView {
  DCHECK(!self.displayedIdentity)
      << base::SysNSStringToUTF8([self description]);
  // The promo on top of the feed is only logged as visible when most of it can
  // be seen, so it can be used without `self.signinPromoViewVisible`.
  DCHECK(self.signinPromoViewVisible ||
         self.accessPoint == signin_metrics::AccessPoint::kNtpFeedTopPromo)
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
    case SigninPromoAction::kSigninWithNoDefaultIdentity:
    case SigninPromoAction::kInstantSignin:
      [self showSigninWithIdentity:nil
                         operation:AuthenticationOperation::kInstantSignin
                       promoAction:promoAction];
      return;
    case SigninPromoAction::kSigninSheet:
      [self showSigninWithIdentity:nil
                         operation:AuthenticationOperation::kSigninOnly
                       promoAction:promoAction];
      return;
    case SigninPromoAction::kReviewAccountSettings:
      NOTREACHED() << "This action is only valid for a signed in account.";
  }
}

- (void)signinPromoViewDidTapPrimaryButtonWithDefaultAccount:
    (SigninPromoView*)signinPromoView {
  DCHECK(self.displayedIdentity) << base::SysNSStringToUTF8([self description]);
  DCHECK(self.signinPromoViewVisible)
      << base::SysNSStringToUTF8([self description]);
  DCHECK(!self.invalidClosedOrNeverVisible)
      << base::SysNSStringToUTF8([self description]);
  switch (self.signinPromoAction) {
    case SigninPromoAction::kInstantSignin:
      [self sendImpressionsTillSigninButtonsHistogram];
      [self showSigninWithIdentity:self.displayedIdentity
                         operation:AuthenticationOperation::kInstantSignin
                       promoAction:signin_metrics::PromoAction::
                                       PROMO_ACTION_WITH_DEFAULT];
      return;
    case SigninPromoAction::kSigninWithNoDefaultIdentity:
    case SigninPromoAction::kSigninSheet:
      [self sendImpressionsTillSigninButtonsHistogram];
      [self showSigninWithIdentity:nil
                         operation:AuthenticationOperation::kSigninOnly
                       promoAction:signin_metrics::PromoAction::
                                       PROMO_ACTION_WITH_DEFAULT];
      return;
    case SigninPromoAction::kReviewAccountSettings:
      if (self.accessPoint == signin_metrics::AccessPoint::kBookmarkManager) {
        base::RecordAction(base::UserMetricsAction(
            "ReviewAccountSettings_Tapped_FromBookmarkManager"));
      } else if (self.accessPoint ==
                 signin_metrics::AccessPoint::kReadingList) {
        base::RecordAction(base::UserMetricsAction(
            "ReviewAccountSettings_Tapped_FromReadingListManager"));
      }
      [self showAccountSettings];
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
    case SigninPromoAction::kSigninSheet:
      [self showSigninWithIdentity:nil
                         operation:AuthenticationOperation::kSigninOnly
                       promoAction:signin_metrics::PromoAction::
                                       PROMO_ACTION_NOT_DEFAULT];
      return;
    case SigninPromoAction::kReviewAccountSettings:
      NOTREACHED() << "This action is only valid for a signed in account.";
    case SigninPromoAction::kSigninWithNoDefaultIdentity:
      NOTREACHED() << "The user should not be able to explicitly select "
                      "\"other account\".";
  }
}

- (void)signinPromoViewCloseButtonWasTapped:(SigninPromoView*)view {
  DCHECK(!self.invalidClosedOrNeverVisible)
      << base::SysNSStringToUTF8([self description]);
  // The promo on top of the feed is only logged as visible when most of it can
  // be seen, so it can be dismissed without `self.signinPromoViewVisible`.
  DCHECK(self.signinPromoViewVisible ||
         self.accessPoint == signin_metrics::AccessPoint::kNtpFeedTopPromo)
      << base::SysNSStringToUTF8([self description]);
  base::RecordAction(base::UserMetricsAction("Signin_Promo_Close"));
  self.signinPromoViewState = SigninPromoViewState::kClosed;
  const char* alreadySeenSigninViewPreferenceKey =
      AlreadySeenSigninViewPreferenceKey(self.accessPoint,
                                         self.signinPromoAction);
  DCHECK(alreadySeenSigninViewPreferenceKey)
      << base::SysNSStringToUTF8([self description]);
  self.prefService->SetBoolean(alreadySeenSigninViewPreferenceKey, true);

  switch (self.signinPromoAction) {
    case SigninPromoAction::kReviewAccountSettings:
      // This promo action should not contribute to the displayed count of the
      // sign-in actions.
      break;
    case SigninPromoAction::kSigninSheet:
    case SigninPromoAction::kInstantSignin:
    case SigninPromoAction::kSigninWithNoDefaultIdentity:
      const char* displayedCountPreferenceKey =
          DisplayedCountPreferenceKey(self.accessPoint);
      if (displayedCountPreferenceKey) {
        int displayedCount =
            self.prefService->GetInteger(displayedCountPreferenceKey);
        RecordImpressionsTilXButtonHistogramForAccessPoint(self.accessPoint,
                                                           displayedCount);
        break;
      }
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
