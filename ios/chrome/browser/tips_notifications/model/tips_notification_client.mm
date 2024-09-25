// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tips_notifications/model/tips_notification_client.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/task/bind_post_task.h"
#import "base/time/time.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/pref_service.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "ios/chrome/browser/default_browser/model/promo_source.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/ntp/model/set_up_list_prefs.h"
#import "ios/chrome/browser/push_notification/model/constants.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/utils/first_run_util.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/docking_promo_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/shared/public/commands/whats_new_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/tips_notifications/model/utils.h"
#import "ios/chrome/browser/ui/authentication/signin_presenter.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/utils.h"
#import "ios/public/provider/chrome/browser/lens/lens_api.h"
#import "ui/base/device_form_factor.h"

namespace {

// The amount of time used to determine if Lens was opened recently.
const base::TimeDelta kLensOpenedRecency = base::Days(30);

// Returns the first notification from `requests` whose identifier matches
// `identifier`.
UNNotificationRequest* NotificationWithIdentifier(
    NSString* identifier,
    NSArray<UNNotificationRequest*>* requests) {
  for (UNNotificationRequest* request in requests) {
    if ([request.identifier isEqualToString:identifier]) {
      return request;
    }
  }
  return nil;
}

// Returns true if signin is allowed / enabled.
bool IsSigninEnabled(AuthenticationService* auth_service) {
  switch (auth_service->GetServiceStatus()) {
    case AuthenticationService::ServiceStatus::SigninForcedByPolicy:
    case AuthenticationService::ServiceStatus::SigninAllowed:
      return true;
    case AuthenticationService::ServiceStatus::SigninDisabledByUser:
    case AuthenticationService::ServiceStatus::SigninDisabledByPolicy:
    case AuthenticationService::ServiceStatus::SigninDisabledByInternal:
      return false;
  }
}

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

// Returns true if the Feature Engagement Tracker has ever triggered for the
// given `feature`.
bool FETHasEverTriggered(Browser* browser, const base::Feature& feature) {
  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForProfile(browser->GetProfile());
  return tracker->HasEverTriggered(feature, true);
}

// Returns the user's type stored in local state prefs.
TipsNotificationUserType GetUserType() {
  PrefService* local_state = GetApplicationContext()->GetLocalState();
  return static_cast<TipsNotificationUserType>(
      local_state->GetInteger(kTipsNotificationsUserType));
}

// Sets the user's type in local state prefs, and records a histogram with the
// type.
void SetUserType(TipsNotificationUserType user_type) {
  PrefService* local_state = GetApplicationContext()->GetLocalState();
  local_state->SetInteger(kTipsNotificationsUserType, int(user_type));
  base::UmaHistogramEnumeration("IOS.Notifications.Tips.UserType", user_type);
}

}  // namespace

TipsNotificationClient::TipsNotificationClient()
    : PushNotificationClient(PushNotificationClientId::kTips) {
  pref_change_registrar_.Init(GetApplicationContext()->GetLocalState());
  PrefChangeRegistrar::NamedChangeCallback pref_callback = base::BindRepeating(
      &TipsNotificationClient::OnPermittedPrefChanged, base::Unretained(this));
  pref_change_registrar_.Add(prefs::kAppLevelPushNotificationPermissions,
                             pref_callback);
  permitted_ = IsPermitted();
  user_type_ = GetUserType();
}

TipsNotificationClient::~TipsNotificationClient() = default;

bool TipsNotificationClient::HandleNotificationInteraction(
    UNNotificationResponse* response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsTipsNotification(response.notification.request)) {
    return false;
  }

  interacted_type_ = ParseTipsNotificationType(response.notification.request);
  if (!interacted_type_.has_value()) {
    base::UmaHistogramEnumeration("IOS.Notifications.Tips.Interaction",
                                  TipsNotificationType::kError);
    return false;
  }
  base::UmaHistogramEnumeration("IOS.Notifications.Tips.Interaction",
                                interacted_type_.value());

  // If the app is not yet foreground active, store the notification type and
  // handle it later when the app becomes foreground active.
  if (IsSceneLevelForegroundActive()) {
    CheckAndMaybeRequestNotification(base::DoNothing());
  }
  return true;
}

void TipsNotificationClient::HandleNotificationInteraction(
    TipsNotificationType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Browser* browser = GetSceneLevelForegroundActiveBrowser();
  CHECK(browser);
  id<ApplicationCommands> application_handler =
      HandlerForProtocol(browser->GetCommandDispatcher(), ApplicationCommands);
  [application_handler
      prepareToPresentModal:
          base::CallbackToBlock(
              base::BindOnce(&TipsNotificationClient::ShowUIForNotificationType,
                             weak_ptr_factory_.GetWeakPtr(), type, browser))];
}

std::optional<UIBackgroundFetchResult>
TipsNotificationClient::HandleNotificationReception(
    NSDictionary<NSString*, id>* userInfo) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (![userInfo objectForKey:kTipsNotificationId]) {
    return std::nullopt;
  }
  return UIBackgroundFetchResultNoData;
}

NSArray<UNNotificationCategory*>*
TipsNotificationClient::RegisterActionableNotifications() {
  return @[];
}

void TipsNotificationClient::OnSceneActiveForegroundBrowserReady() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  OnSceneActiveForegroundBrowserReady(base::DoNothing());
}

void TipsNotificationClient::OnSceneActiveForegroundBrowserReady(
    base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (user_type_ == TipsNotificationUserType::kUnknown) {
    ClassifyUser();
  }
  CheckAndMaybeRequestNotification(std::move(closure));
}

void TipsNotificationClient::CheckAndMaybeRequestNotification(
    base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  permitted_ = IsPermitted();

  if (interacted_type_.has_value()) {
    GetApplicationContext()->GetLocalState()->ClearPref(
        kTipsNotificationsDismissCount);
    HandleNotificationInteraction(interacted_type_.value());
  }

  // If the user hasn't opted-in, exit early to avoid incurring the cost of
  // checking delivered and requested notifications.
  if (!permitted_) {
    std::move(closure).Run();
    return;
  }

  GetPendingRequest(
      base::BindOnce(&TipsNotificationClient::OnPendingRequestFound,
                     weak_ptr_factory_.GetWeakPtr())
          .Then(std::move(closure)));
}

// static
void TipsNotificationClient::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(kTipsNotificationsSentPref, 0);
  registry->RegisterIntegerPref(kTipsNotificationsLastSent, -1);
  registry->RegisterIntegerPref(kTipsNotificationsLastTriggered, -1);
  registry->RegisterTimePref(kTipsNotificationsLastRequestedTime, base::Time());
  registry->RegisterIntegerPref(kTipsNotificationsUserType, 0);
  registry->RegisterIntegerPref(kTipsNotificationsDismissCount, 0);
}

void TipsNotificationClient::GetPendingRequest(
    GetPendingRequestCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto completion = base::CallbackToBlock(base::BindPostTask(
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(&NotificationWithIdentifier, kTipsNotificationId)
          .Then(std::move(callback))));

  [UNUserNotificationCenter.currentNotificationCenter
      getPendingNotificationRequestsWithCompletionHandler:completion];
}

void TipsNotificationClient::OnPendingRequestFound(
    UNNotificationRequest* request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!request) {
    MaybeLogTriggeredNotification();
    MaybeLogDismissedNotification();
    interacted_type_ = std::nullopt;
    MaybeRequestNotification(base::DoNothing());
    return;
  }

  MaybeLogDismissedNotification();
  interacted_type_ = std::nullopt;
}

void TipsNotificationClient::MaybeRequestNotification(
    base::OnceClosure completion) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!permitted_ || DismissLimitReached()) {
    std::move(completion).Run();
    return;
  }

  PrefService* local_state = GetApplicationContext()->GetLocalState();
  int sent_bitfield = local_state->GetInteger(kTipsNotificationsSentPref);
  int enabled_bitfield = TipsNotificationsEnabledBitfield();

  // The types of notifications that could be sent will be evaluated in the
  // order they appear in this array.
  std::vector<TipsNotificationType> types = TipsNotificationsTypesOrder();

  for (TipsNotificationType type : types) {
    int bit = 1 << int(type);
    if (sent_bitfield & bit) {
      // This type of notification has already been sent.
      continue;
    }
    if (!(enabled_bitfield & bit)) {
      // This type of notification is not enabled.
      continue;
    }
    if (ShouldSendNotification(type)) {
      RequestNotification(type, std::move(completion));
      return;
    }
  }
  std::move(completion).Run();
}

void TipsNotificationClient::ClearAllRequestedNotifications() {
  [UNUserNotificationCenter.currentNotificationCenter
      removePendingNotificationRequestsWithIdentifiers:@[
        kTipsNotificationId
      ]];
}

void TipsNotificationClient::RequestNotification(TipsNotificationType type,
                                                 base::OnceClosure completion) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UNNotificationRequest* request = TipsNotificationRequest(type, user_type_);

  auto completion_block = base::CallbackToBlock(base::BindPostTask(
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(&TipsNotificationClient::OnNotificationRequested,
                     weak_ptr_factory_.GetWeakPtr(), type)
          .Then(std::move(completion))));

  [UNUserNotificationCenter.currentNotificationCenter
      addNotificationRequest:request
       withCompletionHandler:completion_block];
  MarkNotificationTypeSent(type);
}

void TipsNotificationClient::OnNotificationRequested(TipsNotificationType type,
                                                     NSError* error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error) {
    base::RecordAction(
        base::UserMetricsAction("IOS.Notifications.Tips.NotSentError"));
  }
}

bool TipsNotificationClient::ShouldSendNotification(TipsNotificationType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
    case TipsNotificationType::kError:
      NOTREACHED();
  }
}

bool TipsNotificationClient::ShouldSendDefaultBrowser() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return !IsChromeLikelyDefaultBrowser() && !DefaultBrowserPromoCanceled();
}

bool TipsNotificationClient::ShouldSendWhatsNew() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Browser* browser = GetSceneLevelForegroundActiveBrowser();
  if (!browser) {
    return false;
  }
  return !FETHasEverTriggered(browser,
                              feature_engagement::kIPHWhatsNewUpdatedFeature);
}

bool TipsNotificationClient::ShouldSendSignin() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Browser* browser = GetSceneLevelForegroundActiveBrowser();
  if (!browser) {
    return false;
  }
  ProfileIOS* profile = browser->GetProfile();
  AuthenticationService* auth_service =
      AuthenticationServiceFactory::GetForProfile(profile);

  return IsSigninEnabled(auth_service) &&
         !auth_service->HasPrimaryIdentity(signin::ConsentLevel::kSignin);
}

bool TipsNotificationClient::ShouldSendSetUpListContinuation() {
  Browser* browser = GetSceneLevelForegroundActiveBrowser();
  if (!browser) {
    return false;
  }
  PrefService* local_prefs = GetApplicationContext()->GetLocalState();
  PrefService* user_prefs = browser->GetProfile()->GetPrefs();
  if (!set_up_list_utils::IsSetUpListActive(local_prefs, user_prefs)) {
    return false;
  }

  // The Set Up List only shows for 14 days after FirstRun, so this
  // notification should only be requested 14 days minus the trigger interval
  // after FirstRun.
  if (!IsFirstRunRecent(base::Days(14) -
                        TipsNotificationTriggerDelta(user_type_))) {
    return false;
  }
  return !set_up_list_prefs::AllItemsComplete(local_prefs);
}

bool TipsNotificationClient::ShouldSendDocking() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Browser* browser = GetSceneLevelForegroundActiveBrowser();
  if (!browser) {
    return false;
  }
  return !FETHasEverTriggered(browser,
                              feature_engagement::kIPHiOSDockingPromoFeature) &&
         !FETHasEverTriggered(
             browser,
             feature_engagement::kIPHiOSDockingPromoRemindMeLaterFeature);
}

bool TipsNotificationClient::ShouldSendOmniboxPosition() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // OmniboxPositionChoice is only available on phones.
  if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_PHONE) {
    return false;
  }
  return !GetApplicationContext()->GetLocalState()->GetUserPrefValue(
      prefs::kBottomOmnibox);
}

bool TipsNotificationClient::ShouldSendLens() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Early return if Lens is not available.
  if (!ios::provider::IsLensSupported()) {
    return false;
  }

  base::Time last_opened =
      GetApplicationContext()->GetLocalState()->GetTime(prefs::kLensLastOpened);
  return base::Time::Now() - last_opened > kLensOpenedRecency;
}

bool TipsNotificationClient::ShouldSendEnhancedSafeBrowsing() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Browser* browser = GetSceneLevelForegroundActiveBrowser();
  if (!browser) {
    return false;
  }
  PrefService* user_prefs = browser->GetProfile()->GetPrefs();
  return !safe_browsing::IsEnhancedProtectionEnabled(*user_prefs);
}

bool TipsNotificationClient::IsSceneLevelForegroundActive() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetSceneLevelForegroundActiveBrowser() != nullptr;
}

void TipsNotificationClient::ShowUIForNotificationType(
    TipsNotificationType type,
    Browser* browser) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (type) {
    case TipsNotificationType::kDefaultBrowser:
      ShowDefaultBrowserPromo(browser);
      break;
    case TipsNotificationType::kWhatsNew:
      ShowWhatsNew(browser);
      break;
    case TipsNotificationType::kSignin:
      ShowSignin(browser);
      break;
    case TipsNotificationType::kSetUpListContinuation:
      ShowSetUpListContinuation(browser);
      break;
    case TipsNotificationType::kDocking:
      ShowDocking(browser);
      break;
    case TipsNotificationType::kOmniboxPosition:
      ShowOmniboxPosition(browser);
      break;
    case TipsNotificationType::kLens:
      ShowLensPromo(browser);
      break;
    case TipsNotificationType::kEnhancedSafeBrowsing:
      ShowEnhancedSafeBrowsingPromo(browser);
      break;
    case TipsNotificationType::kError:
      NOTREACHED();
  }
}

void TipsNotificationClient::ShowDefaultBrowserPromo(Browser* browser) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  id<SettingsCommands> settings_handler =
      HandlerForProtocol(browser->GetCommandDispatcher(), SettingsCommands);
  [settings_handler
      showDefaultBrowserSettingsFromViewController:nil
                                      sourceForUMA:
                                          DefaultBrowserSettingsPageSource::
                                              kTipsNotification];
}

void TipsNotificationClient::ShowWhatsNew(Browser* browser) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  [HandlerForProtocol(browser->GetCommandDispatcher(), WhatsNewCommands)
      showWhatsNew];
}

void TipsNotificationClient::ShowSignin(Browser* browser) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If there are 0 identities, kInstantSignin requires less taps.
  ProfileIOS* profile = browser->GetProfile();
  AuthenticationOperation operation =
      ChromeAccountManagerServiceFactory::GetForProfile(profile)
              ->HasIdentities()
          ? AuthenticationOperation::kSigninOnly
          : AuthenticationOperation::kInstantSignin;
  ShowSigninCommand* command = [[ShowSigninCommand alloc]
      initWithOperation:operation
               identity:nil
            accessPoint:signin_metrics::AccessPoint::
                            ACCESS_POINT_TIPS_NOTIFICATION
            promoAction:signin_metrics::PromoAction::
                            PROMO_ACTION_NO_SIGNIN_PROMO
               callback:nil];

  [HandlerForProtocol(browser->GetCommandDispatcher(), SigninPresenter)
      showSignin:command];
}

void TipsNotificationClient::ShowSetUpListContinuation(Browser* browser) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  [HandlerForProtocol(browser->GetCommandDispatcher(),
                      ContentSuggestionsCommands)
      showSetUpListSeeMoreMenuExpanded:YES];
}

void TipsNotificationClient::ShowDocking(Browser* browser) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  [HandlerForProtocol(browser->GetCommandDispatcher(), DockingPromoCommands)
      showDockingPromo:YES];
}

void TipsNotificationClient::ShowOmniboxPosition(Browser* browser) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  [HandlerForProtocol(browser->GetCommandDispatcher(),
                      BrowserCoordinatorCommands) showOmniboxPositionChoice];
}

void TipsNotificationClient::ShowLensPromo(Browser* browser) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  [HandlerForProtocol(browser->GetCommandDispatcher(),
                      BrowserCoordinatorCommands) showLensPromo];
}

void TipsNotificationClient::ShowEnhancedSafeBrowsingPromo(Browser* browser) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  [HandlerForProtocol(browser->GetCommandDispatcher(),
                      BrowserCoordinatorCommands)
      showEnhancedSafeBrowsingPromo];
}

void TipsNotificationClient::MarkNotificationTypeSent(
    TipsNotificationType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PrefService* local_state = GetApplicationContext()->GetLocalState();
  int sent_bitfield = local_state->GetInteger(kTipsNotificationsSentPref);
  sent_bitfield |= 1 << int(type);
  local_state->SetInteger(kTipsNotificationsSentPref, sent_bitfield);
  local_state->SetInteger(kTipsNotificationsLastSent, int(type));
  local_state->SetTime(kTipsNotificationsLastRequestedTime, base::Time::Now());
  base::UmaHistogramEnumeration("IOS.Notifications.Tips.Sent", type);
}

void TipsNotificationClient::MarkNotificationTypeNotSent(
    TipsNotificationType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PrefService* local_state = GetApplicationContext()->GetLocalState();
  int sent_bitfield = local_state->GetInteger(kTipsNotificationsSentPref);
  sent_bitfield &= ~(1 << int(type));
  local_state->SetInteger(kTipsNotificationsSentPref, sent_bitfield);
  local_state->ClearPref(kTipsNotificationsLastSent);
}

void TipsNotificationClient::MaybeLogTriggeredNotification() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PrefService* local_state = GetApplicationContext()->GetLocalState();
  const PrefService::Preference* last_sent =
      local_state->FindPreference(kTipsNotificationsLastSent);
  if (last_sent->IsDefaultValue()) {
    return;
  }

  TipsNotificationType type =
      static_cast<TipsNotificationType>(last_sent->GetValue()->GetInt());
  base::UmaHistogramEnumeration("IOS.Notifications.Tips.Triggered", type);
  local_state->SetInteger(kTipsNotificationsLastTriggered, int(type));
  local_state->ClearPref(kTipsNotificationsLastSent);
}

void TipsNotificationClient::MaybeLogDismissedNotification() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PrefService* local_state = GetApplicationContext()->GetLocalState();
  if (interacted_type_.has_value()) {
    local_state->ClearPref(kTipsNotificationsLastTriggered);
    return;
  }
  const PrefService::Preference* last_triggered =
      local_state->FindPreference(kTipsNotificationsLastTriggered);
  if (last_triggered->IsDefaultValue()) {
    return;
  }

  auto completion = base::CallbackToBlock(base::BindPostTask(
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(&TipsNotificationClient::OnGetDeliveredNotifications,
                     weak_ptr_factory_.GetWeakPtr())));
  [UNUserNotificationCenter.currentNotificationCenter
      getDeliveredNotificationsWithCompletionHandler:completion];
}

void TipsNotificationClient::OnGetDeliveredNotifications(
    NSArray<UNNotification*>* notifications) {
  for (UNNotification* notification in notifications) {
    if ([notification.request.identifier isEqualToString:kTipsNotificationId]) {
      return;
    }
  }
  // No notification was found, so it must have been dismissed.
  PrefService* local_state = GetApplicationContext()->GetLocalState();
  int dismiss_count =
      local_state->GetInteger(kTipsNotificationsDismissCount) + 1;
  local_state->SetInteger(kTipsNotificationsDismissCount, dismiss_count);
  TipsNotificationType type = static_cast<TipsNotificationType>(
      local_state->GetInteger(kTipsNotificationsLastTriggered));
  base::UmaHistogramEnumeration("IOS.Notifications.Tips.Dismissed", type);
  local_state->ClearPref(kTipsNotificationsLastTriggered);
}

bool TipsNotificationClient::IsPermitted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/325279788): use
  // GetMobileNotificationPermissionStatusForClient to determine opt-in
  // state.
  PrefService* local_state = GetApplicationContext()->GetLocalState();
  return local_state->GetDict(prefs::kAppLevelPushNotificationPermissions)
      .FindBool(kTipsNotificationKey)
      .value_or(false);
}

bool TipsNotificationClient::DismissLimitReached() {
  int dismiss_limit = TipsNotificationsDismissLimit();
  if (!dismiss_limit) {
    return false;
  }

  int dismiss_count = GetApplicationContext()->GetLocalState()->GetInteger(
      kTipsNotificationsDismissCount);
  return dismiss_count >= dismiss_limit;
}

void TipsNotificationClient::OnPermittedPrefChanged(const std::string& name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool newpermitted_ = IsPermitted();
  if (permitted_ != newpermitted_ && IsSceneLevelForegroundActive()) {
    ClearAllRequestedNotifications();
    CheckAndMaybeRequestNotification(base::DoNothing());
  }
}

void TipsNotificationClient::ClassifyUser() {
  PrefService* local_state = GetApplicationContext()->GetLocalState();

  if (!local_state->GetUserPrefValue(kTipsNotificationsLastRequestedTime)) {
    return;
  }

  base::Time now = base::Time::Now();
  base::Time last_request =
      local_state->GetTime(kTipsNotificationsLastRequestedTime);
  if (now < last_request + base::Hours(2)) {
    // Not enough time has passed to classify the user.
    return;
  }

  if (now > last_request + TipsNotificationTriggerDelta(
                               TipsNotificationUserType::kUnknown)) {
    user_type_ = TipsNotificationUserType::kLessEngaged;
  } else {
    user_type_ = TipsNotificationUserType::kActiveSeeker;
  }
  SetUserType(user_type_);
}
