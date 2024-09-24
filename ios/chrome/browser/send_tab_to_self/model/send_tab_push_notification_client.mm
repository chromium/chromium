// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/send_tab_to_self/model/send_tab_push_notification_client.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "components/send_tab_to_self/send_tab_to_self_model.h"
#import "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#import "ios/chrome/browser/push_notification/model/constants.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/browser/push_notification/model/push_notification_util.h"
#import "ios/chrome/browser/send_tab_to_self/model/send_tab_to_self_browser_agent.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/sync/model/send_tab_to_self_sync_service_factory.h"
#import "url/gurl.h"

namespace {

// Key for the sent url in the notification payload.
NSString* const kUrlKey = @"url";

// Key for the GUID (unique identifier) of the associated SendTabEntry.
NSString* const kGuidKey = @"SendTabGuid";

}  // namespace

SendTabPushNotificationClient::SendTabPushNotificationClient()
    : PushNotificationClient(PushNotificationClientId::kSendTab) {}

SendTabPushNotificationClient::~SendTabPushNotificationClient() = default;

bool SendTabPushNotificationClient::HandleNotificationInteraction(
    UNNotificationResponse* response) {
  NSDictionary* user_info = response.notification.request.content.userInfo;
  DCHECK(user_info);

  if ([user_info[kPushNotificationClientIdKey] intValue] !=
      static_cast<int>(PushNotificationClientId::kSendTab)) {
    return false;
  }

  std::string url = base::SysNSStringToUTF8(user_info[kUrlKey]);

  // Load URL in a new tab and mark the corresponding SendTabToSelfEntry as
  // opened.
  std::string guid = base::SysNSStringToUTF8(
      response.notification.request.content.userInfo[kGuidKey]);
  LoadUrlInNewTab(GURL(url), base::BindOnce(^(Browser* browser) {
                    send_tab_to_self::SendTabToSelfModel* send_tab_model =
                        SendTabToSelfSyncServiceFactory::GetForProfile(
                            browser->GetProfile())
                            ->GetSendTabToSelfModel();
                    send_tab_model->MarkEntryOpened(guid);
                  }));

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
