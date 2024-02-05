// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tips_notifications/model/tips_notification_client.h"

#import "base/task/bind_post_task.h"
#import "base/time/time.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state_manager.h"
#import "ios/chrome/browser/shared/model/utils/first_run_util.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/promos_manager_commands.h"
#import "ios/chrome/browser/tips_notifications/model/utils.h"

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

}  // namespace

TipsNotificationClient::TipsNotificationClient()
    : PushNotificationClient(PushNotificationClientId::kTips) {}

TipsNotificationClient::~TipsNotificationClient() = default;

void TipsNotificationClient::HandleNotificationInteraction(
    UNNotificationResponse* response) {
  if (!IsTipsNotification(response.notification.request)) {
    return;
  }

  interacted_type_ = ParseTipsNotificationType(response.notification.request);
  if (!interacted_type_.has_value()) {
    // TODO(crbug.com/1519157): Add logging for this error condition.
    return;
  }

  // If the app is not yet foreground active, store the notification type and
  // handle it later when the app becomes foreground active.
  if (IsSceneLevelForegroundActive()) {
    HandleNotificationInteraction(interacted_type_.value());
    interacted_type_ = std::nullopt;
  }
}

void TipsNotificationClient::HandleNotificationInteraction(
    TipsNotificationType type) {
  switch (type) {
    case TipsNotificationType::kDefaultBrowser:
      ShowDefaultBrowserPromo();
      break;
    case TipsNotificationType::kWhatsNew:
      ShowWhatsNew();
      break;
    case TipsNotificationType::kSignin:
      // TODO(crbug.com/1517912) implement Signin interaction.
      break;
  }
}

UIBackgroundFetchResult TipsNotificationClient::HandleNotificationReception(
    NSDictionary<NSString*, id>* notification) {
  return UIBackgroundFetchResultNoData;
}

NSArray<UNNotificationCategory*>*
TipsNotificationClient::RegisterActionableNotifications() {
  return @[];
}

void TipsNotificationClient::OnSceneActiveForegroundBrowserReady() {
  OnSceneActiveForegroundBrowserReady(base::DoNothing());
}

void TipsNotificationClient::OnSceneActiveForegroundBrowserReady(
    base::OnceClosure closure) {
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
  auto completion = base::CallbackToBlock(base::BindPostTask(
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(&NotificationWithIdentifier, kTipsNotificationId)
          .Then(std::move(callback))));

  [UNUserNotificationCenter.currentNotificationCenter
      getPendingNotificationRequestsWithCompletionHandler:completion];
}

void TipsNotificationClient::ClearNotification(base::OnceClosure callback) {
  GetPendingRequest(
      base::BindOnce(&TipsNotificationClient::OnNotificationCleared,
                     weak_ptr_factory_.GetWeakPtr())
          .Then(std::move(callback)));
}

void TipsNotificationClient::OnNotificationCleared(
    UNNotificationRequest* request) {
  if (!request) {
    return;
  }

  std::optional<TipsNotificationType> type = ParseTipsNotificationType(request);
  if (type.has_value()) {
    MarkNotificationTypeNotSent(type.value());
  }
  [UNUserNotificationCenter.currentNotificationCenter
      removePendingNotificationRequestsWithIdentifiers:@[
        kTipsNotificationId
      ]];
}

void TipsNotificationClient::MaybeRequestNotification() {
  if (!IsFirstRunRecent(base::Days(14))) {
    return;
  }

  PrefService* local_state = GetApplicationContext()->GetLocalState();
  int sent_bitfield = local_state->GetInteger(kTipsNotificationsSentPref);

  // The types of notifications that could be sent will be evaluated in the
  // order they appear in this array.
  static const TipsNotificationType kTypes[] = {
      TipsNotificationType::kDefaultBrowser,
      TipsNotificationType::kWhatsNew,
      TipsNotificationType::kSignin,
  };

  for (TipsNotificationType type : kTypes) {
    if (sent_bitfield & (1 << int(type))) {
      // This type of notification has already been sent.
      continue;
    }
    if (ShouldSendNotification(type)) {
      RequestNotification(type);
      break;
    }
  }
}

void TipsNotificationClient::RequestNotification(TipsNotificationType type) {
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
  if (!error) {
    MarkNotificationTypeSent(type);
  }
  // TODO(crbug.com/1519157): Add logging if there is an
  // error.
}

bool TipsNotificationClient::ShouldSendNotification(TipsNotificationType type) {
  switch (type) {
    case TipsNotificationType::kDefaultBrowser:
      return !IsChromeLikelyDefaultBrowser();
    case TipsNotificationType::kWhatsNew:
      return true;
    case TipsNotificationType::kSignin:
      return true;
  }
}

Browser* TipsNotificationClient::GetSceneLevelForegroundActiveBrowser() {
  ChromeBrowserState* browser_state = GetApplicationContext()
                                          ->GetChromeBrowserStateManager()
                                          ->GetLastUsedBrowserState();
  BrowserList* browser_list =
      BrowserListFactory::GetForBrowserState(browser_state);
  for (Browser* browser : browser_list->AllRegularBrowsers()) {
    if (!browser->IsInactive()) {
      if (browser->GetSceneState().activationLevel ==
          SceneActivationLevelForegroundActive) {
        return browser;
      }
    }
  }
  return nullptr;
}

bool TipsNotificationClient::IsSceneLevelForegroundActive() {
  return GetSceneLevelForegroundActiveBrowser() != nullptr;
}

void TipsNotificationClient::ShowDefaultBrowserPromo() {
  raw_ptr<Browser> browser = GetSceneLevelForegroundActiveBrowser();
  [HandlerForProtocol(browser->GetCommandDispatcher(), PromosManagerCommands)
      maybeDisplayDefaultBrowserPromo];
}

void TipsNotificationClient::ShowWhatsNew() {
  raw_ptr<Browser> browser = GetSceneLevelForegroundActiveBrowser();
  [HandlerForProtocol(browser->GetCommandDispatcher(),
                      BrowserCoordinatorCommands) showWhatsNew];
}

void TipsNotificationClient::MarkNotificationTypeSent(
    TipsNotificationType type) {
  PrefService* local_state = GetApplicationContext()->GetLocalState();
  int sent_bitfield = local_state->GetInteger(kTipsNotificationsSentPref);
  sent_bitfield |= 1 << int(type);
  local_state->SetInteger(kTipsNotificationsSentPref, sent_bitfield);
}

void TipsNotificationClient::MarkNotificationTypeNotSent(
    TipsNotificationType type) {
  PrefService* local_state = GetApplicationContext()->GetLocalState();
  int sent_bitfield = local_state->GetInteger(kTipsNotificationsSentPref);
  sent_bitfield &= ~(1 << int(type));
  local_state->SetInteger(kTipsNotificationsSentPref, sent_bitfield);
}
