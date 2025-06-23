// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tips_notifications/model/tips_notification_client.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/bind_post_task.h"
#import "base/time/time.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/push_notification/model/constants.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client.h"
#import "ios/chrome/browser/push_notification/model/push_notification_service.h"
#import "ios/chrome/browser/push_notification/model/push_notification_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/utils/first_run_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/tips_notifications/model/tips_notification_criteria.h"
#import "ios/chrome/browser/tips_notifications/model/tips_notification_presenter.h"
#import "ios/chrome/browser/tips_notifications/model/utils.h"

namespace {

// The amount of time used to determine if the user should be classified.
const base::TimeDelta kClassifyUserRecency = base::Hours(2);

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

// Returns true if `time` is less time ago than `delta`.
bool IsRecent(base::Time time, base::TimeDelta delta) {
  return base::Time::Now() - time < delta;
}

}  // namespace

TipsNotificationClient::TipsNotificationClient()
    : PushNotificationClient(PushNotificationClientId::kTips,
                             PushNotificationClientScope::kAppWide) {
  local_state_ = GetApplicationContext()->GetLocalState();
  pref_change_registrar_.Init(local_state_);
  PrefChangeRegistrar::NamedChangeCallback pref_callback = base::BindRepeating(
      &TipsNotificationClient::OnPermittedPrefChanged, base::Unretained(this));
  pref_change_registrar_.Add(prefs::kAppLevelPushNotificationPermissions,
                             pref_callback);
  PrefChangeRegistrar::NamedChangeCallback auth_pref_callback =
      base::BindRepeating(&TipsNotificationClient::OnAuthPrefChanged,
                          base::Unretained(this));
  pref_change_registrar_.Add(prefs::kPushNotificationAuthorizationStatus,
                             auth_pref_callback);
  permitted_ = IsPermitted();
  user_type_ = GetTipsNotificationUserType(local_state_);
}

TipsNotificationClient::~TipsNotificationClient() = default;

bool TipsNotificationClient::CanHandleNotification(
    UNNotification* notification) {
  return IsTipsNotification(notification.request);
}

bool TipsNotificationClient::HandleNotificationInteraction(
    UNNotificationResponse* response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CanHandleNotification(response.notification)) {
    return false;
  }

  interacted_type_ = ParseTipsNotificationType(response.notification.request);
  if (!interacted_type_.has_value()) {
    base::UmaHistogramEnumeration("IOS.Notifications.Tips.Interaction",
                                  TipsNotificationType::kError);
    return false;
  }
  const char* histogram =
      IsProactiveTipsNotification(response.notification.request)
          ? "IOS.Notifications.Tips.Proactive.Interaction"
          : "IOS.Notifications.Tips.Interaction";
  base::UmaHistogramEnumeration(histogram, interacted_type_.value());

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
  Browser* browser = GetActiveForegroundBrowser();
  CHECK(browser);
  TipsNotificationPresenter::Present(browser->AsWeakPtr(), type);

  // If a relevant feature is enabled and the user hasn't yet opted-in, and the
  // current auth status is "authorized", interacting with a notification (which
  // must have been sent provisionally) will be treated as a positive signal to
  // opt in the user to this type of notification.
  if ((IsProvisionalNotificationAlertEnabled() ||
       IsIOSReactivationNotificationsEnabled()) &&
      !permitted_) {
    [PushNotificationUtil
        getPermissionSettings:base::CallbackToBlock(base::BindOnce(
                                  &TipsNotificationClient::OptInIfAuthorized,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  browser->GetProfile()->AsWeakPtr()))];
  }
}

void TipsNotificationClient::OptInIfAuthorized(
    base::WeakPtr<ProfileIOS> weak_profile,
    UNNotificationSettings* settings) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (settings.authorizationStatus != UNAuthorizationStatusAuthorized) {
    return;
  }
  ProfileIOS* profile = weak_profile.get();
  if (!profile) {
    return;
  }

  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForProfile(profile);
  id<SystemIdentity> identity =
      authService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  const std::string& gaiaID = base::SysNSStringToUTF8(identity.gaiaID);
  PushNotificationService* service =
      GetApplicationContext()->GetPushNotificationService();
  // Set `permitted_` here so that the OnPermittedPrefChanged exits early.
  permitted_ = true;
  service->SetPreference(base::SysUTF8ToNSString(gaiaID),
                         PushNotificationClientId::kTips, true);
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
  UpdateProvisionalAllowed();
  forced_type_ = ForcedTipsNotificationType();
  if (user_type_ == TipsNotificationUserType::kUnknown &&
      !CanSendReactivation()) {
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
  if (!permitted_ && !CanSendReactivation()) {
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
  registry->RegisterIntegerPref(kReactivationNotificationsCanceledCount, 0);
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

  if (CanSendReactivation()) {
    ClearAllRequestedNotifications();
    std::optional<TipsNotificationType> type =
        ParseTipsNotificationType(request);
    if (type.has_value()) {
      MarkNotificationTypeNotSent(type.value());
      // Increment the Reactivation canceled count.
      int canceled_count =
          local_state_->GetInteger(kReactivationNotificationsCanceledCount) + 1;
      local_state_->SetInteger(kReactivationNotificationsCanceledCount,
                               canceled_count);
    }
    MaybeRequestNotification(base::DoNothing());
  }
}

void TipsNotificationClient::MaybeRequestNotification(
    base::OnceClosure completion) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if ((!permitted_ && !CanSendReactivation())) {
    std::move(completion).Run();
    return;
  }

  Browser* browser = GetActiveForegroundBrowser();
  if (!browser) {
    std::move(completion).Run();
    return;
  }
  ProfileIOS* profile = browser->GetProfile();

  if (forced_type_.has_value()) {
    RequestNotification(forced_type_.value(), profile->GetProfileName(),
                        std::move(completion));
    return;
  }

  int sent_bitfield = local_state_->GetInteger(kTipsNotificationsSentPref);
  int enabled_bitfield = TipsNotificationsEnabledBitfield();

  // The types of notifications that could be sent will be evaluated in the
  // order they appear in this array.
  std::vector<TipsNotificationType> types =
      TipsNotificationsTypesOrder(CanSendReactivation());

  std::unique_ptr<TipsNotificationCriteria> criteria =
      std::make_unique<TipsNotificationCriteria>(profile, local_state_,
                                                 CanSendReactivation());
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
    if (criteria->ShouldSendNotification(type)) {
      RequestNotification(type, profile->GetProfileName(),
                          std::move(completion));
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
                                                 std::string_view profile_name,
                                                 base::OnceClosure completion) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (IsNotificationCollisionManagementEnabled()) {
    ScheduledNotificationRequest request = {
        kTipsNotificationId,
        ContentForTipsNotificationType(type, CanSendReactivation(),
                                       profile_name),
        TipsNotificationTriggerDelta(CanSendReactivation(), user_type_)};
    CheckRateLimitBeforeSchedulingNotification(
        request,
        base::BindPostTask(
            base::SequencedTaskRunner::GetCurrentDefault(),
            base::BindOnce(&TipsNotificationClient::OnNotificationRequested,
                           weak_ptr_factory_.GetWeakPtr(), type)
                .Then(std::move(completion))));
    MarkNotificationTypeSent(type);
    return;
  }

  UNNotificationRequest* request = [UNNotificationRequest
      requestWithIdentifier:kTipsNotificationId
                    content:ContentForTipsNotificationType(
                                type, CanSendReactivation(), profile_name)
                    trigger:[UNTimeIntervalNotificationTrigger
                                triggerWithTimeInterval:
                                    TipsNotificationTriggerDelta(
                                        CanSendReactivation(), user_type_)
                                        .InSecondsF()
                                                repeats:NO]];

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

bool TipsNotificationClient::IsSceneLevelForegroundActive() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetActiveForegroundBrowser() != nullptr;
}

void TipsNotificationClient::MarkNotificationTypeSent(
    TipsNotificationType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  int sent_bitfield = local_state_->GetInteger(kTipsNotificationsSentPref);
  sent_bitfield |= 1 << int(type);
  local_state_->SetInteger(kTipsNotificationsSentPref, sent_bitfield);
  local_state_->SetInteger(kTipsNotificationsLastSent, int(type));
  local_state_->SetTime(kTipsNotificationsLastRequestedTime, base::Time::Now());
  const char* histogram = CanSendReactivation()
                              ? "IOS.Notifications.Tips.Proactive.Sent"
                              : "IOS.Notifications.Tips.Sent";
  base::UmaHistogramEnumeration(histogram, type);
}

void TipsNotificationClient::MarkNotificationTypeNotSent(
    TipsNotificationType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  int sent_bitfield = local_state_->GetInteger(kTipsNotificationsSentPref);
  sent_bitfield &= ~(1 << int(type));
  local_state_->SetInteger(kTipsNotificationsSentPref, sent_bitfield);
  local_state_->ClearPref(kTipsNotificationsLastSent);
}

void TipsNotificationClient::MaybeLogTriggeredNotification() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const PrefService::Preference* last_sent =
      local_state_->FindPreference(kTipsNotificationsLastSent);
  if (last_sent->IsDefaultValue()) {
    return;
  }

  TipsNotificationType type =
      static_cast<TipsNotificationType>(last_sent->GetValue()->GetInt());
  const char* triggered_histogram =
      CanSendReactivation() ? "IOS.Notifications.Tips.Proactive.Triggered"
                            : "IOS.Notifications.Tips.Triggered";
  base::UmaHistogramEnumeration(triggered_histogram, type);
  base::UmaHistogramEnumeration("IOS.Notification.Received",
                                NotificationTypeForTipsNotificationType(type));
  local_state_->SetInteger(kTipsNotificationsLastTriggered, int(type));
  local_state_->ClearPref(kTipsNotificationsLastSent);
}

void TipsNotificationClient::MaybeLogDismissedNotification() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (interacted_type_.has_value()) {
    local_state_->ClearPref(kTipsNotificationsLastTriggered);
    return;
  }
  const PrefService::Preference* last_triggered =
      local_state_->FindPreference(kTipsNotificationsLastTriggered);
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
  int dismiss_count =
      local_state_->GetInteger(kTipsNotificationsDismissCount) + 1;
  local_state_->SetInteger(kTipsNotificationsDismissCount, dismiss_count);
  TipsNotificationType type = static_cast<TipsNotificationType>(
      local_state_->GetInteger(kTipsNotificationsLastTriggered));
  const char* dismissed_histogram =
      CanSendReactivation() ? "IOS.Notifications.Tips.Proactive.Dismissed"
                            : "IOS.Notifications.Tips.Dismissed";
  base::UmaHistogramEnumeration(dismissed_histogram, type);
  local_state_->ClearPref(kTipsNotificationsLastTriggered);
}

bool TipsNotificationClient::IsPermitted() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/325279788): use
  // GetMobileNotificationPermissionStatusForClient to determine opt-in
  // state.
  return local_state_->GetDict(prefs::kAppLevelPushNotificationPermissions)
      .FindBool(kTipsNotificationKey)
      .value_or(false);
}

bool TipsNotificationClient::CanSendReactivation() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If the user has opted-in for Tips, or First-Run was more than 4 weeks ago,
  // or if the feature is not enabled, Reactivation notifications should not
  // be sent.
  if (permitted_ || !IsFirstRunRecent(base::Days(28)) ||
      !IsIOSReactivationNotificationsEnabled() || !provisional_allowed_) {
    return false;
  }

  UNAuthorizationStatus auth_status =
      [PushNotificationUtil getSavedPermissionSettings];
  if (auth_status != UNAuthorizationStatusProvisional) {
    return false;
  }

  return local_state_->GetInteger(kReactivationNotificationsCanceledCount) <
             2 ||
         forced_type_.has_value();
}

void TipsNotificationClient::UpdateProvisionalAllowed() {
  Browser* browser = GetActiveForegroundBrowser();
  CHECK(browser);
  provisional_allowed_ = [PushNotificationUtil
      provisionalAllowedByPolicyForProfile:browser->GetProfile()];
}

void TipsNotificationClient::OnPermittedPrefChanged(const std::string& name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool newpermitted_ = IsPermitted();
  if (permitted_ != newpermitted_) {
    ClearAllRequestedNotifications();
    if (IsSceneLevelForegroundActive()) {
      CheckAndMaybeRequestNotification(base::DoNothing());
    }
  }
}

void TipsNotificationClient::OnAuthPrefChanged(const std::string& name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UNAuthorizationStatus auth_status =
      [PushNotificationUtil getSavedPermissionSettings];
  if (IsSceneLevelForegroundActive() &&
      auth_status == UNAuthorizationStatusProvisional) {
    CheckAndMaybeRequestNotification(base::DoNothing());
  }
}

void TipsNotificationClient::ClassifyUser() {
  if (!local_state_->GetUserPrefValue(kTipsNotificationsLastRequestedTime)) {
    return;
  }

  base::Time last_request =
      local_state_->GetTime(kTipsNotificationsLastRequestedTime);
  if (IsRecent(last_request, kClassifyUserRecency)) {
    // Not enough time has passed to classify the user.
    return;
  }

  base::TimeDelta trigger_delta = TipsNotificationTriggerDelta(
      CanSendReactivation(), TipsNotificationUserType::kUnknown);
  if (IsRecent(last_request, trigger_delta)) {
    user_type_ = TipsNotificationUserType::kActiveSeeker;
  } else {
    user_type_ = TipsNotificationUserType::kLessEngaged;
  }
  SetTipsNotificationUserType(local_state_, user_type_);
  base::UmaHistogramEnumeration("IOS.Notifications.Tips.UserType", user_type_);
}
