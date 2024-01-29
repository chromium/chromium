// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tips_notifications/model/tips_notification_client.h"

#import "base/time/time.h"
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
  if (interacted_type_.has_value()) {
    HandleNotificationInteraction(interacted_type_.value());
    interacted_type_ = std::nullopt;
  }
  ClearNotification();
  MaybeRequestNotification();
}

void TipsNotificationClient::ClearNotification() {
  UNUserNotificationCenter* notificationCenter =
      [UNUserNotificationCenter currentNotificationCenter];
  [notificationCenter removePendingNotificationRequestsWithIdentifiers:@[
    kTipsNotificationId
  ]];
}

void TipsNotificationClient::MaybeRequestNotification() {
  if (!IsFirstRunRecent(base::Days(14))) {
    return;
  }

  // The types of notifications that could be sent will be evaluated in the
  // order they appear in this array.
  static const TipsNotificationType kTypes[] = {
      TipsNotificationType::kDefaultBrowser,
      TipsNotificationType::kWhatsNew,
      TipsNotificationType::kSignin,
  };

  for (TipsNotificationType type : kTypes) {
    if (ShouldSendNotification(type)) {
      RequestNotification(type);
      break;
    }
  }
}

void TipsNotificationClient::RequestNotification(TipsNotificationType type) {
  UNUserNotificationCenter* notificationCenter =
      [UNUserNotificationCenter currentNotificationCenter];
  UNNotificationRequest* request = TipsNotificationRequest(type);
  [notificationCenter
      addNotificationRequest:request
       withCompletionHandler:^(NSError* error){
           // TODO(crbug.com/1519157): Add logging if there is an
           // error.
       }];
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
