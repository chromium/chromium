// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/send_tab_to_self/model/send_tab_push_notification_client.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "components/prefs/pref_service.h"
#import "components/send_tab_to_self/send_tab_to_self_model.h"
#import "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#import "ios/chrome/browser/push_notification/model/constants.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/browser/push_notification/model/push_notification_prefs.h"
#import "ios/chrome/browser/push_notification/model/push_notification_service.h"
#import "ios/chrome/browser/push_notification/model/push_notification_settings_util.h"
#import "ios/chrome/browser/push_notification/model/push_notification_util.h"
#import "ios/chrome/browser/send_tab_to_self/model/send_tab_to_self_browser_agent.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/features.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/sync/model/send_tab_to_self_sync_service_factory.h"
#import "url/gurl.h"

namespace {

// Key for the sent url in the notification payload.
NSString* const kUrlKey = @"url";

// Key for the GUID (unique identifier) of the associated SendTabEntry.
NSString* const kGuidKey = @"SendTabGuid";

}  // namespace

SendTabPushNotificationClient::SendTabPushNotificationClient(
    ProfileIOS* profile)
    : PushNotificationClient(PushNotificationClientId::kSendTab, profile) {
  CHECK(IsMultiProfilePushNotificationHandlingEnabled());
}

SendTabPushNotificationClient::SendTabPushNotificationClient()
    : PushNotificationClient(PushNotificationClientId::kSendTab,
                             PushNotificationClientScope::kPerProfile) {
  CHECK(!IsMultiProfilePushNotificationHandlingEnabled());
}

SendTabPushNotificationClient::~SendTabPushNotificationClient() = default;

std::optional<NotificationType>
SendTabPushNotificationClient::GetNotificationType(
    UNNotification* notification) {
  if (CanHandleNotification(notification)) {
    return NotificationType::kSendTab;
  }
  return std::nullopt;
}

bool SendTabPushNotificationClient::CanHandleNotification(
    UNNotification* notification) {
  NSDictionary* user_info = notification.request.content.userInfo;
  return [user_info[kPushNotificationClientIdKey] intValue] ==
         static_cast<int>(PushNotificationClientId::kSendTab);
}

bool SendTabPushNotificationClient::HandleNotificationInteraction(
    UNNotificationResponse* response) {
  NSDictionary* user_info = response.notification.request.content.userInfo;
  CHECK(user_info);

  if (!CanHandleNotification(response.notification)) {
    return false;
  }

  std::string url = base::SysNSStringToUTF8(user_info[kUrlKey]);

  // Load URL in a new tab and mark the corresponding SendTabToSelfEntry as
  // opened.
  std::string guid = base::SysNSStringToUTF8(
      response.notification.request.content.userInfo[kGuidKey]);
  LoadUrlInNewTab(
      GURL(url),
      base::BindOnce(&SendTabPushNotificationClient::OnURLLoadedInNewTab,
                     weak_ptr_factory_.GetWeakPtr(), std::move(guid)));

  if (IsNotificationCollisionManagementEnabled()) {
    GetApplicationContext()->GetLocalState()->SetTime(
        push_notification_prefs::kSendTabLastOpenTimestamp, base::Time::Now());
  }

  base::RecordAction(
      base::UserMetricsAction("IOS.Notifications.SendTab.Interaction"));

  return true;
}

std::optional<UIBackgroundFetchResult>
SendTabPushNotificationClient::HandleNotificationReception(
    NSDictionary<NSString*, id>* notification) {
  UNAuthorizationStatus authStatus =
      [PushNotificationUtil getSavedPermissionSettings];
  push_notification::SettingsAuthorizationStatus settingsAuthStatus =
      [PushNotificationUtil getNotificationSettingsStatusFrom:authStatus];
  base::UmaHistogramEnumeration("IOS.Notifications.SendTab.Received",
                                settingsAuthStatus);
  return UIBackgroundFetchResultNoData;
}

NSArray<UNNotificationCategory*>*
SendTabPushNotificationClient::RegisterActionableNotifications() {
  return @[];
}

void SendTabPushNotificationClient::OnURLLoadedInNewTab(std::string guid,
                                                        Browser* browser) {
  send_tab_to_self::SendTabToSelfModel* send_tab_model =
      SendTabToSelfSyncServiceFactory::GetForProfile(browser->GetProfile())
          ->GetSendTabToSelfModel();
  send_tab_model->MarkEntryOpened(guid);

  if (IsProvisionalNotificationAlertEnabled()) {
    AuthenticationService* authService =
        AuthenticationServiceFactory::GetForProfile(browser->GetProfile());
    id<SystemIdentity> identity =
        authService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
    if (!push_notification_settings::
            GetMobileNotificationPermissionStatusForClient(
                PushNotificationClientId::kSendTab, identity.gaiaId)) {
      PushNotificationService* service =
          GetApplicationContext()->GetPushNotificationService();
      service->SetPreference(identity.gaiaId,
                             PushNotificationClientId::kSendTab, true);
    }
  }
}
