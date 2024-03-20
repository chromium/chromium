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
#import "components/sync/base/features.h"
#import "ios/chrome/browser/default_browser/model/promo_source.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/push_notification/model/constants.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state_manager.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/utils/first_run_util.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/tips_notifications/model/utils.h"
#import "ios/chrome/browser/ui/authentication/signin_presenter.h"

namespace {

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

}  // namespace

TipsNotificationClient::TipsNotificationClient()
    : PushNotificationClient(PushNotificationClientId::kTips) {
  pref_change_registrar_.Init(GetApplicationContext()->GetLocalState());
  PrefChangeRegistrar::NamedChangeCallback pref_callback = base::BindRepeating(
      &TipsNotificationClient::OnPermittedPrefChanged, base::Unretained(this));
  pref_change_registrar_.Add(prefs::kAppLevelPushNotificationPermissions,
                             pref_callback);
  permitted_ = IsPermitted();
}

TipsNotificationClient::~TipsNotificationClient() = default;

void TipsNotificationClient::HandleNotificationInteraction(
    UNNotificationResponse* response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsTipsNotification(response.notification.request)) {
    return;
  }

  interacted_type_ = ParseTipsNotificationType(response.notification.request);
  if (!interacted_type_.has_value()) {
    base::UmaHistogramEnumeration("IOS.Notifications.Tips.Interaction",
                                  TipsNotificationType::kError);
    return;
  }
  base::UmaHistogramEnumeration("IOS.Notifications.Tips.Interaction",
                                interacted_type_.value());

  // If the app is not yet foreground active, store the notification type and
  // handle it later when the app becomes foreground active.
  if (IsSceneLevelForegroundActive()) {
    HandleNotificationInteraction(interacted_type_.value());
    interacted_type_ = std::nullopt;
    ClearAndMaybeRequestNotification(base::DoNothing());
  }
}

void TipsNotificationClient::HandleNotificationInteraction(
    TipsNotificationType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (type) {
    case TipsNotificationType::kDefaultBrowser:
      ShowDefaultBrowserPromo();
      break;
    case TipsNotificationType::kWhatsNew:
      ShowWhatsNew();
      break;
    case TipsNotificationType::kSignin:
      ShowSignin();
      break;
    case TipsNotificationType::kError:
      NOTREACHED();
      break;
  }
}

UIBackgroundFetchResult TipsNotificationClient::HandleNotificationReception(
    NSDictionary<NSString*, id>* notification) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
  ClearAndMaybeRequestNotification(std::move(closure));
}

void TipsNotificationClient::ClearAndMaybeRequestNotification(
    base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  permitted_ = IsPermitted();
  if (interacted_type_.has_value()) {
    HandleNotificationInteraction(interacted_type_.value());
    interacted_type_ = std::nullopt;
  }
  ClearNotification(
      base::BindOnce(&TipsNotificationClient::MaybeRequestNotification,
                     weak_ptr_factory_.GetWeakPtr())
          .Then(std::move(closure)));
}

// static
void TipsNotificationClient::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(kTipsNotificationsSentPref, 0);
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

void TipsNotificationClient::ClearNotification(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  GetPendingRequest(
      base::BindOnce(&TipsNotificationClient::OnNotificationCleared,
                     weak_ptr_factory_.GetWeakPtr())
          .Then(std::move(callback)));
}

void TipsNotificationClient::OnNotificationCleared(
    UNNotificationRequest* request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!request) {
    return;
  }

  std::optional<TipsNotificationType> type = ParseTipsNotificationType(request);
  if (type.has_value()) {
    MarkNotificationTypeNotSent(type.value());
    base::UmaHistogramEnumeration("IOS.Notifications.Tips.Cleared",
                                  type.value());
  }
  [UNUserNotificationCenter.currentNotificationCenter
      removePendingNotificationRequestsWithIdentifiers:@[
        kTipsNotificationId
      ]];
}

void TipsNotificationClient::MaybeRequestNotification() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsFirstRunRecent(base::Days(14)) || !permitted_) {
    return;
  }

  PrefService* local_state = GetApplicationContext()->GetLocalState();
  int sent_bitfield = local_state->GetInteger(kTipsNotificationsSentPref);
  int enabled_bitfield = TipsNotificationsEnabledBitfield();

  // The types of notifications that could be sent will be evaluated in the
  // order they appear in this array.
  static const TipsNotificationType kTypes[] = {
      TipsNotificationType::kDefaultBrowser,
      TipsNotificationType::kWhatsNew,
      TipsNotificationType::kSignin,
  };

  for (TipsNotificationType type : kTypes) {
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
      RequestNotification(type);
      break;
    }
  }
}

void TipsNotificationClient::RequestNotification(TipsNotificationType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UNNotificationRequest* request = TipsNotificationRequest(type);

  auto completion = base::CallbackToBlock(base::BindPostTask(
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(&TipsNotificationClient::OnNotificationRequested,
                     weak_ptr_factory_.GetWeakPtr(), type)));

  [UNUserNotificationCenter.currentNotificationCenter
      addNotificationRequest:request
       withCompletionHandler:completion];
}

void TipsNotificationClient::OnNotificationRequested(TipsNotificationType type,
                                                     NSError* error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!error) {
    MarkNotificationTypeSent(type);
  } else {
    base::RecordAction(
        base::UserMetricsAction("IOS.Notifications.Tips.NotSentError"));
  }
}

bool TipsNotificationClient::ShouldSendNotification(TipsNotificationType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (type) {
    case TipsNotificationType::kDefaultBrowser:
      return !IsChromeLikelyDefaultBrowser();
    case TipsNotificationType::kWhatsNew:
      return ShouldSendWhatsNew();
    case TipsNotificationType::kSignin:
      return ShouldSendSignin();
    case TipsNotificationType::kError:
      NOTREACHED_NORETURN();
  }
}

bool TipsNotificationClient::ShouldSendWhatsNew() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Browser* browser = GetSceneLevelForegroundActiveBrowser();
  if (!browser) {
    return false;
  }
  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForBrowserState(
          browser->GetBrowserState());
  return !tracker->HasEverTriggered(
      feature_engagement::kIPHWhatsNewUpdatedFeature, true);
}

bool TipsNotificationClient::ShouldSendSignin() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Browser* browser = GetSceneLevelForegroundActiveBrowser();
  if (!browser) {
    return false;
  }
  ChromeBrowserState* browser_state = browser->GetBrowserState();
  AuthenticationService* auth_service =
      AuthenticationServiceFactory::GetForBrowserState(browser_state);

  return IsSigninEnabled(auth_service) &&
         !auth_service->HasPrimaryIdentity(signin::ConsentLevel::kSignin);
}

bool TipsNotificationClient::IsSceneLevelForegroundActive() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetSceneLevelForegroundActiveBrowser() != nullptr;
}

void TipsNotificationClient::ShowDefaultBrowserPromo() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Browser* browser = GetSceneLevelForegroundActiveBrowser();
  id<ApplicationCommands> application_handler =
      HandlerForProtocol(browser->GetCommandDispatcher(), ApplicationCommands);
  [application_handler prepareToPresentModal:^{
    id<SettingsCommands> settings_handler =
        HandlerForProtocol(browser->GetCommandDispatcher(), SettingsCommands);
    [settings_handler
        showDefaultBrowserSettingsFromViewController:nil
                                        sourceForUMA:
                                            DefaultBrowserSettingsPageSource::
                                                kTipsNotification];
  }];
}

void TipsNotificationClient::ShowWhatsNew() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Browser* browser = GetSceneLevelForegroundActiveBrowser();
  id<ApplicationCommands> application_handler =
      HandlerForProtocol(browser->GetCommandDispatcher(), ApplicationCommands);
  [application_handler prepareToPresentModal:^{
    [HandlerForProtocol(browser->GetCommandDispatcher(),
                        BrowserCoordinatorCommands) showWhatsNew];
  }];
}

void TipsNotificationClient::ShowSignin() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Browser* browser = GetSceneLevelForegroundActiveBrowser();
  AuthenticationOperation operation = AuthenticationOperation::kSigninAndSync;
  if (base::FeatureList::IsEnabled(
          syncer::kReplaceSyncPromosWithSignInPromos)) {
    // If there are identities, kInstantSignin requires less taps.
    ChromeBrowserState* browser_state = browser->GetBrowserState();
    operation =
        ChromeAccountManagerServiceFactory::GetForBrowserState(browser_state)
                ->HasIdentities()
            ? AuthenticationOperation::kSigninOnly
            : AuthenticationOperation::kInstantSignin;
  }
  ShowSigninCommand* command = [[ShowSigninCommand alloc]
      initWithOperation:operation
               identity:nil
            accessPoint:signin_metrics::AccessPoint::
                            ACCESS_POINT_TIPS_NOTIFICATION
            promoAction:signin_metrics::PromoAction::
                            PROMO_ACTION_NO_SIGNIN_PROMO
               callback:nil];

  id<ApplicationCommands> application_handler =
      HandlerForProtocol(browser->GetCommandDispatcher(), ApplicationCommands);
  [application_handler prepareToPresentModal:^{
    [HandlerForProtocol(browser->GetCommandDispatcher(), SigninPresenter)
        showSignin:command];
  }];
}

void TipsNotificationClient::MarkNotificationTypeSent(
    TipsNotificationType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PrefService* local_state = GetApplicationContext()->GetLocalState();
  int sent_bitfield = local_state->GetInteger(kTipsNotificationsSentPref);
  sent_bitfield |= 1 << int(type);
  local_state->SetInteger(kTipsNotificationsSentPref, sent_bitfield);
  base::UmaHistogramEnumeration("IOS.Notifications.Tips.Sent", type);
}

void TipsNotificationClient::MarkNotificationTypeNotSent(
    TipsNotificationType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PrefService* local_state = GetApplicationContext()->GetLocalState();
  int sent_bitfield = local_state->GetInteger(kTipsNotificationsSentPref);
  sent_bitfield &= ~(1 << int(type));
  local_state->SetInteger(kTipsNotificationsSentPref, sent_bitfield);
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

void TipsNotificationClient::OnPermittedPrefChanged(const std::string& name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool newpermitted_ = IsPermitted();
  if (permitted_ != newpermitted_) {
    ClearAndMaybeRequestNotification(base::DoNothing());
  }
}
