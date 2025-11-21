// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tips_notifications/model/tips_notification_criteria.h"

#import "base/time/time.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/omnibox/browser/omnibox_pref_names.h"
#import "components/password_manager/core/browser/password_manager_util.h"
#import "components/prefs/pref_service.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "components/search/search.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/set_up_list/public/set_up_list_utils.h"
#import "ios/chrome/browser/default_browser/model/promo_source.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/lens/ui_bundled/lens_availability.h"
#import "ios/chrome/browser/ntp/model/features.h"
#import "ios/chrome/browser/ntp/model/set_up_list_prefs.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/utils/first_run_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/tips_notifications/model/utils.h"
#import "ui/base/device_form_factor.h"

using base::Time;
using base::TimeDelta;

namespace {

// The amount of time used to determine if Lens was opened recently.
const TimeDelta kLensOpenedRecency = base::Days(30);
// The amount of time used to determine if the user used Lens Overlay recently.
const TimeDelta kLensOverlayRecency = base::Days(30);
// The amount of time used to determine if the CPE promo was displayed recently.
const TimeDelta kCPEPromoRecency = base::Days(7);
// The amount of time used to determine if the user successfully logged in
// recently.
const TimeDelta kSuccessfullLoginRecency = base::Days(30);

// Returns true if a Default Browser Promo was canceled.
bool DefaultBrowserPromoCanceled() {
  std::optional<IOSDefaultBrowserPromoAction> action =
      DefaultBrowserPromoLastAction();
  if (!action.has_value()) {
    return false;
  }

  switch (action.value()) {
    case IOSDefaultBrowserPromoAction::kCancel:
      return true;
    case IOSDefaultBrowserPromoAction::kActionButton:
    case IOSDefaultBrowserPromoAction::kRemindMeLater:
    case IOSDefaultBrowserPromoAction::kDismiss:
      return false;
  }
}

// Returns true if `time` is less time ago than `delta`.
bool IsRecent(base::Time time, TimeDelta delta) {
  return base::Time::Now() - time < delta;
}

}  // namespace

TipsNotificationCriteria::TipsNotificationCriteria(ProfileIOS* profile,
                                                   PrefService* local_state,
                                                   bool reactivation)
    : profile_(profile),
      profile_prefs_(profile->GetPrefs()),
      local_state_(local_state),
      reactivation_(reactivation) {}

bool TipsNotificationCriteria::ShouldSendNotification(
    TipsNotificationType type) {
  switch (type) {
    case TipsNotificationType::kDefaultBrowser:
      return ShouldSendDefaultBrowser();
    case TipsNotificationType::kWhatsNew:
      return ShouldSendWhatsNew();
    case TipsNotificationType::kSignin:
      return ShouldSendSignin();
    case TipsNotificationType::kSetUpListContinuation:
      return ShouldSendSetUpListContinuation();
    case TipsNotificationType::kDocking:
      return ShouldSendDocking();
    case TipsNotificationType::kOmniboxPosition:
      return ShouldSendOmniboxPosition();
    case TipsNotificationType::kLens:
      return ShouldSendLens();
    case TipsNotificationType::kEnhancedSafeBrowsing:
      return ShouldSendEnhancedSafeBrowsing();
    case TipsNotificationType::kCPE:
      return ShouldSendCPE();
    case TipsNotificationType::kLensOverlay:
      return ShouldSendLensOverlay();
    case TipsNotificationType::kTrustedVaultKeyRetrieval:
      return ShouldSendTrustedVaultKeyRetrieval();
    case TipsNotificationType::kIncognitoLock:
    case TipsNotificationType::kError:
      NOTREACHED();
  }
}

bool TipsNotificationCriteria::CanSignIn(ProfileIOS* profile) {
  AuthenticationService* auth_service =
      AuthenticationServiceFactory::GetForProfile(profile);
  return auth_service->SigninEnabled() &&
         !auth_service->HasPrimaryIdentity(signin::ConsentLevel::kSignin);
}

bool TipsNotificationCriteria::ShouldSendDefaultBrowser() {
  if (IsChromeLikelyDefaultBrowser()) {
    return false;
  }
  if (base::FeatureList::IsEnabled(kIOSOneTimeDefaultBrowserNotification)) {
    // For kIOSOneTimeDefaultBrowserNotification, the logic simply checks if
    // the user has not seen the promo in the last 14 days.
    feature_engagement::Tracker* tracker =
        feature_engagement::TrackerFactory::GetForProfile(profile_);
    bool would_trigger = tracker->WouldTriggerHelpUI(
        feature_engagement::kIPHiOSOneTimeDefaultBrowserNotificationFeature);
    if (would_trigger) {
      return true;
    }
  }
  return !DefaultBrowserPromoCanceled();
}

bool TipsNotificationCriteria::ShouldSendWhatsNew() {
  return !FETHasEverTriggered(feature_engagement::kIPHWhatsNewUpdatedFeature);
}

bool TipsNotificationCriteria::ShouldSendSignin() {
  return CanSignIn(profile_);
}

bool TipsNotificationCriteria::ShouldSendSetUpListContinuation() {
  if (!set_up_list_utils::IsSetUpListActive(local_state_, profile_prefs_)) {
    return false;
  }

  // This notification should only be requested during the duration of the Set
  // Up List minus the trigger interval after FirstRun.
  TimeDelta trigger_delta = TipsNotificationTriggerDelta(
      reactivation_, GetTipsNotificationUserType(local_state_));
  if (!IsFirstRunRecent(set_up_list::SetUpListDurationPastFirstRun() -
                        trigger_delta)) {
    return false;
  }
  return !set_up_list_prefs::AllItemsComplete(local_state_);
}

bool TipsNotificationCriteria::ShouldSendDocking() {
  return !FETHasEverTriggered(feature_engagement::kIPHiOSDockingPromoFeature) &&
         !FETHasEverTriggered(
             feature_engagement::kIPHiOSDockingPromoRemindMeLaterFeature);
}

bool TipsNotificationCriteria::ShouldSendOmniboxPosition() {
  // OmniboxPositionChoice is only available on phones.
  if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_PHONE) {
    return false;
  }
  return !local_state_->GetUserPrefValue(omnibox::kIsOmniboxInBottomPosition);
}

bool TipsNotificationCriteria::ShouldSendLens() {
  // Early return if Lens is not available or disabled by policy.
  TemplateURLService* template_url_service =
      ios::TemplateURLServiceFactory::GetForProfile(profile_);
  bool default_search_is_google =
      search::DefaultSearchProviderIsGoogle(template_url_service);
  const bool lens_enabled =
      lens_availability::CheckAndLogAvailabilityForLensEntryPoint(
          LensEntrypoint::NewTabPage, default_search_is_google);
  if (!lens_enabled) {
    return false;
  }

  base::Time last_opened = local_state_->GetTime(prefs::kLensLastOpened);
  return !IsRecent(last_opened, kLensOpenedRecency);
}

bool TipsNotificationCriteria::ShouldSendEnhancedSafeBrowsing() {
  return profile_prefs_->GetBoolean(prefs::kAdvancedProtectionAllowed) &&
         !safe_browsing::IsEnhancedProtectionEnabled(*profile_prefs_);
}

bool TipsNotificationCriteria::ShouldSendCPE() {
  if (!local_state_->GetBoolean(
          prefs::kIosCredentialProviderPromoPolicyEnabled)) {
    return false;
  }
  bool is_credential_provider_enabled =
      password_manager_util::IsCredentialProviderEnabledOnStartup(local_state_);
  if (is_credential_provider_enabled) {
    return false;
  }
  base::Time promo_display_time =
      local_state_->GetTime(prefs::kIosCredentialProviderPromoDisplayTime);
  if (IsRecent(promo_display_time, kCPEPromoRecency)) {
    return false;
  }
  base::Time login_time =
      local_state_->GetTime(prefs::kIosSuccessfulLoginWithExistingPassword);
  return IsRecent(login_time, kSuccessfullLoginRecency);
}

bool TipsNotificationCriteria::ShouldSendLensOverlay() {
  base::Time lens_overlay_last_presented =
      local_state_->GetTime(prefs::kLensOverlayLastPresented);
  return !IsRecent(lens_overlay_last_presented, kLensOverlayRecency);
}

bool TipsNotificationCriteria::ShouldSendTrustedVaultKeyRetrieval() {
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfileIfExists(profile_);
  if (sync_service == nullptr) {
    // Sync service might be not available for some profiles. If Sync service is
    // not available, we don't need to do display this notification.
    return false;
  }
  if (!IsIOSTrustedVaultNotificationEnabled()) {
    return false;
  }
  return sync_service->GetUserSettings()
      ->IsTrustedVaultKeyRequiredForPreferredDataTypes();
}

bool TipsNotificationCriteria::FETHasEverTriggered(
    const base::Feature& feature) {
  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForProfile(profile_);
  return tracker->HasEverTriggered(feature, true);
}
