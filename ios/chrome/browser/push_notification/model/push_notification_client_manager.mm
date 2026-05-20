// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/push_notification/model/push_notification_client_manager.h"

#import <Foundation/Foundation.h>

#import <optional>
#import <vector>

#import "base/feature_list.h"
#import "base/strings/sys_string_conversions.h"
#import "components/desktop_to_mobile_promos/features.h"
#import "components/optimization_guide/core/optimization_guide_features.h"
#import "components/send_tab_to_self/features.h"
#import "components/sharing_message/features.h"
#import "ios/chrome/browser/commerce/model/push_notification/commerce_push_notification_client.h"
#import "ios/chrome/browser/commerce/model/push_notification/push_notification_feature.h"
#import "ios/chrome/browser/content_notification/model/content_notification_client.h"
#import "ios/chrome/browser/cross_platform_promos/model/cross_platform_promos_notification_client.h"
#import "ios/chrome/browser/push_notification/model/constants.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/browser/push_notification/model/push_notification_util.h"
#import "ios/chrome/browser/reminder_notifications/model/reminder_notification_client.h"
#import "ios/chrome/browser/safety_check_notifications/model/safety_check_notification_client.h"
#import "ios/chrome/browser/send_tab_to_self/model/send_tab_push_notification_client.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/features.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/utils/first_run_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/tips_notifications/model/tips_notification_client.h"

using send_tab_to_self::AreIOSTabRemindersEnabled;

PushNotificationClientManager::PushNotificationClientManager(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    ProfileIOS* profile)
    : task_runner_(std::move(task_runner)), profile_(profile) {
  CHECK(task_runner_);
  CHECK(profile_);
  CHECK(IsMultiProfilePushNotificationHandlingEnabled());

  AddPerProfilePushNotificationClients();
}

PushNotificationClientManager::PushNotificationClientManager(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)), profile_(nullptr) {
  CHECK(task_runner_);

  AddAppWidePushNotificationClients();

  if (!IsMultiProfilePushNotificationHandlingEnabled()) {
    AddPerProfilePushNotificationClients();
  }
}

PushNotificationClientManager::~PushNotificationClientManager() = default;

void PushNotificationClientManager::AddPushNotificationClient(
    std::unique_ptr<PushNotificationClient> client) {
  clients_.insert(std::make_pair(client->GetClientId(), std::move(client)));
}

void PushNotificationClientManager::RemovePushNotificationClient(
    PushNotificationClientId client_id) {
  clients_.erase(client_id);
}

std::vector<const PushNotificationClient*>
PushNotificationClientManager::GetPushNotificationClients() {
  std::vector<const PushNotificationClient*> manager_clients;

  for (auto& client : clients_) {
    manager_clients.push_back(std::move(client.second.get()));
  }

  return manager_clients;
}

void PushNotificationClientManager::HandleNotificationInteraction(
    UNNotificationResponse* notification_response) {
  std::optional<PushNotificationClientId> clientId = [PushNotificationUtil
      mapToPushNotificationClientIdFromUserInfo:notification_response
                                                    .notification.request
                                                    .content.userInfo];
  if (clientId.has_value()) {
    auto it = clients_.find(clientId.value());
    if (it != clients_.end()) {
      it->second->HandleNotificationInteraction(notification_response);
    }
  } else {
    // Safety until all clients have incorporated the appropriate ids into their
    // payload.
    for (auto& client : clients_) {
      client.second->HandleNotificationInteraction(notification_response);
    }
  }
}

UIBackgroundFetchResult
PushNotificationClientManager::HandleNotificationReception(
    NSDictionary<NSString*, id>* user_info) {
  if (user_info == nil || user_info == nullptr) {
    return UIBackgroundFetchResultFailed;
  }
  std::optional<PushNotificationClientId> clientId = [PushNotificationUtil
      mapToPushNotificationClientIdFromUserInfo:user_info];
  std::optional<UIBackgroundFetchResult> client_result;
  if (clientId.has_value()) {
    auto it = clients_.find(clientId.value());
    if (it != clients_.end()) {
      client_result = it->second->HandleNotificationReception(user_info);
    }
  } else {
    for (auto& client : clients_) {
      client_result = client.second->HandleNotificationReception(user_info);
      if (client_result.has_value()) {
        break;
      }
    }
  }
  if (client_result.has_value()) {
    return client_result.value();
  }
  return UIBackgroundFetchResultNoData;
}

void PushNotificationClientManager::RegisterActionableNotifications() {
  NSMutableSet* categorySet = [[NSMutableSet alloc] init];

  for (auto& client : clients_) {
    NSArray<UNNotificationCategory*>* client_categories =
        client.second->RegisterActionableNotifications();

    for (id category in client_categories) {
      [categorySet addObject:category];
    }
  }

  [PushNotificationUtil registerActionableNotifications:categorySet];
}

std::vector<PushNotificationClientId>
PushNotificationClientManager::GetClients() {
  std::vector<PushNotificationClientId> client_ids = {
      PushNotificationClientId::kCommerce, PushNotificationClientId::kTips};
  client_ids.push_back(PushNotificationClientId::kContent);
  client_ids.push_back(PushNotificationClientId::kSports);
  client_ids.push_back(PushNotificationClientId::kSafetyCheck);
    client_ids.push_back(PushNotificationClientId::kSendTab);
    if (AreIOSTabRemindersEnabled()) {
      client_ids.push_back(PushNotificationClientId::kReminders);
    }
  if (IsMobilePromoOnDesktopNotificationsEnabled()) {
    client_ids.push_back(PushNotificationClientId::kCrossPlatformPromos);
  }
  return client_ids;
}

void PushNotificationClientManager::OnSceneActiveForegroundBrowserReady() {
  for (auto& client : clients_) {
    client.second->OnSceneActiveForegroundBrowserReady();
  }
  MaybeTriggerForcedNotification();
}

// Adds clients that operate on a per-Profile basis.
void PushNotificationClientManager::AddPerProfilePushNotificationClients() {
  if (optimization_guide::features::IsPushNotificationsEnabled()) {
    std::unique_ptr<CommercePushNotificationClient> client;

    if (IsMultiProfilePushNotificationHandlingEnabled()) {
      CHECK(profile_);

      client = std::make_unique<CommercePushNotificationClient>(profile_);
    } else {
      client = std::make_unique<CommercePushNotificationClient>();
    }

    CHECK_EQ(client->GetClientScope(),
             PushNotificationClientScope::kPerProfile);

    AddPushNotificationClient(std::move(client));
  }

  std::unique_ptr<ContentNotificationClient> content_notification_client;

  if (IsMultiProfilePushNotificationHandlingEnabled()) {
    CHECK(profile_);

    content_notification_client =
        std::make_unique<ContentNotificationClient>(profile_);
  } else {
    content_notification_client = std::make_unique<ContentNotificationClient>();
  }

  CHECK_EQ(content_notification_client->GetClientScope(),
           PushNotificationClientScope::kPerProfile);

  AddPushNotificationClient(std::move(content_notification_client));

  if (IsMultiProfilePushNotificationHandlingEnabled() && profile_) {
    // Pass profile and task runner for multi-profile handling.
    auto client =
        std::make_unique<SafetyCheckNotificationClient>(profile_, task_runner_);
    CHECK_EQ(client->GetClientScope(),
             PushNotificationClientScope::kPerProfile);
    AddPushNotificationClient(std::move(client));
  } else {
    // Pass only task runner for single-profile or default handling.
    auto client = std::make_unique<SafetyCheckNotificationClient>(task_runner_);
    CHECK_EQ(client->GetClientScope(),
             PushNotificationClientScope::kPerProfile);
    AddPushNotificationClient(std::move(client));
  }

  // Add Send Tab To Self client.
  std::unique_ptr<SendTabPushNotificationClient> send_tab_client;

  if (IsMultiProfilePushNotificationHandlingEnabled()) {
    CHECK(profile_);

    send_tab_client = std::make_unique<SendTabPushNotificationClient>(profile_);
  } else {
    send_tab_client = std::make_unique<SendTabPushNotificationClient>();
  }

  CHECK_EQ(send_tab_client->GetClientScope(),
           PushNotificationClientScope::kPerProfile);

  AddPushNotificationClient(std::move(send_tab_client));

  // Additionally, add Reminder client if STTS reminders are also enabled.
  if (AreIOSTabRemindersEnabled() &&
      IsMultiProfilePushNotificationHandlingEnabled()) {
    CHECK(profile_);

    std::unique_ptr<ReminderNotificationClient> reminder_client =
        std::make_unique<ReminderNotificationClient>(profile_);

    CHECK_EQ(reminder_client->GetClientScope(),
             PushNotificationClientScope::kPerProfile);

    AddPushNotificationClient(std::move(reminder_client));
  }
  if (IsMobilePromoOnDesktopNotificationsEnabled() &&
      IsMultiProfilePushNotificationHandlingEnabled()) {
    std::unique_ptr<CrossPlatformPromosNotificationClient> client =
        std::make_unique<CrossPlatformPromosNotificationClient>(profile_);
    AddPushNotificationClient(std::move(client));
  }
}

// Adds clients that operate app-wide.
void PushNotificationClientManager::AddAppWidePushNotificationClients() {
  auto client = std::make_unique<TipsNotificationClient>();
  CHECK_EQ(client->GetClientScope(), PushNotificationClientScope::kAppWide);
  AddPushNotificationClient(std::move(client));
}

PushNotificationClient* PushNotificationClientManager::GetClientForNotification(
    UNNotification* notification) {
  std::optional<PushNotificationClientId> clientId = [PushNotificationUtil
      mapToPushNotificationClientIdFromUserInfo:notification.request.content
                                                    .userInfo];
  if (clientId.has_value()) {
    auto it = clients_.find(clientId.value());
    if (it != clients_.end()) {
      return it->second.get();
    }
  } else {
    // Safety until all clients have incorporated the appropriate ids into their
    // payload.
    for (auto& it : clients_) {
      if (it.second->CanHandleNotification(notification)) {
        return it.second.get();
      }
    }
  }
  return nullptr;
}

void PushNotificationClientManager::MaybeTriggerForcedNotification() {
  int type = experimental_flags::GetForcedPushNotificationType();
  if (type == 0) {
    return;
  }

  // Prevent infinite trigger loops by only scheduling the forced notification
  // once per app execution session.
  static bool has_triggered_in_current_session = false;
  if (has_triggered_in_current_session) {
    return;
  }

  PushNotificationClientId client_id =
      static_cast<PushNotificationClientId>(type);
  auto client_it = clients_.find(client_id);
  if (client_it == clients_.end()) {
    return;
  }

  // The subtype preference key is dynamically resolved using the format
  // ForcedPushNotificationSubtype_<TypeID>, where <TypeID> is the integer
  // value of PushNotificationClientId (e.g. ForcedPushNotificationSubtype_8).
  NSString* subtype_key =
      [NSString stringWithFormat:@"ForcedPushNotificationSubtype_%d", type];
  int subtype =
      [[NSUserDefaults standardUserDefaults] integerForKey:subtype_key];

  NSMutableDictionary* user_info = [NSMutableDictionary dictionary];
  user_info[kPushNotificationClientIdKey] = @(type);
  if (profile_) {
    std::string name = profile_->GetProfileName();
    user_info[kOriginatingProfileNameKey] = base::SysUTF8ToNSString(name);
  }

  std::optional<ForcedNotificationPayload> payload =
      client_it->second->BuildForcedNotificationPayload(subtype, user_info);
  if (!payload.has_value()) {
    return;
  }

  UNMutableNotificationContent* content =
      [[UNMutableNotificationContent alloc] init];
  content.title = payload->title;
  content.body = payload->body;
  content.sound = [UNNotificationSound defaultSound];
  content.userInfo = user_info;

  int delay = experimental_flags::GetForcedPushNotificationDelay();
  UNTimeIntervalNotificationTrigger* local_trigger =
      [UNTimeIntervalNotificationTrigger
          triggerWithTimeInterval:std::max(1, delay)
                          repeats:NO];

  UNNotificationRequest* request =
      [UNNotificationRequest requestWithIdentifier:[[NSUUID UUID] UUIDString]
                                           content:content
                                           trigger:local_trigger];

  [[UNUserNotificationCenter currentNotificationCenter]
      addNotificationRequest:request
       withCompletionHandler:nil];

  has_triggered_in_current_session = true;
}
