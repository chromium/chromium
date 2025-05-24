// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/push_notification/model/push_notification_client_manager.h"

#import <Foundation/Foundation.h>

#import <optional>
#import <vector>

#import "base/feature_list.h"
#import "components/optimization_guide/core/optimization_guide_features.h"
#import "components/send_tab_to_self/features.h"
#import "ios/chrome/browser/commerce/model/push_notification/commerce_push_notification_client.h"
#import "ios/chrome/browser/commerce/model/push_notification/push_notification_feature.h"
#import "ios/chrome/browser/content_notification/model/content_notification_client.h"
#import "ios/chrome/browser/push_notification/model/constants.h"
#import "ios/chrome/browser/push_notification/model/push_notification_util.h"
#import "ios/chrome/browser/reminder_notifications/model/reminder_notification_client.h"
#import "ios/chrome/browser/safety_check_notifications/model/safety_check_notification_client.h"
#import "ios/chrome/browser/send_tab_to_self/model/send_tab_push_notification_client.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/features.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/utils/first_run_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/tips_notifications/model/tips_notification_client.h"

using send_tab_to_self::IsSendTabIOSPushNotificationsEnabledWithTabReminders;

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
  if (IsContentNotificationExperimentEnabled()) {
    client_ids.push_back(PushNotificationClientId::kContent);
    client_ids.push_back(PushNotificationClientId::kSports);
  }
  if (IsSafetyCheckNotificationsEnabled()) {
    client_ids.push_back(PushNotificationClientId::kSafetyCheck);
  }
  if (base::FeatureList::IsEnabled(
          send_tab_to_self::kSendTabToSelfIOSPushNotifications)) {
    client_ids.push_back(PushNotificationClientId::kSendTab);
  }
  return client_ids;
}

void PushNotificationClientManager::OnSceneActiveForegroundBrowserReady() {
  for (auto& client : clients_) {
    client.second->OnSceneActiveForegroundBrowserReady();
  }
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

  if (IsContentNotificationExperimentEnabled()) {
    std::unique_ptr<ContentNotificationClient> client;

    if (IsMultiProfilePushNotificationHandlingEnabled()) {
      CHECK(profile_);

      client = std::make_unique<ContentNotificationClient>(profile_);
    } else {
      client = std::make_unique<ContentNotificationClient>();
    }

    CHECK_EQ(client->GetClientScope(),
             PushNotificationClientScope::kPerProfile);

    AddPushNotificationClient(std::move(client));
  }

  if (IsSafetyCheckNotificationsEnabled()) {
    if (IsMultiProfilePushNotificationHandlingEnabled() && profile_) {
      // Pass profile and task runner for multi-profile handling.
      auto client = std::make_unique<SafetyCheckNotificationClient>(
          profile_, task_runner_);
      CHECK_EQ(client->GetClientScope(),
               PushNotificationClientScope::kPerProfile);
      AddPushNotificationClient(std::move(client));
    } else {
      // Pass only task runner for single-profile or default handling.
      auto client =
          std::make_unique<SafetyCheckNotificationClient>(task_runner_);
      CHECK_EQ(client->GetClientScope(),
               PushNotificationClientScope::kPerProfile);
      AddPushNotificationClient(std::move(client));
    }
  }

  // Add Send Tab To Self client if its push notifications are enabled.
  if (base::FeatureList::IsEnabled(
          send_tab_to_self::kSendTabToSelfIOSPushNotifications)) {
    std::unique_ptr<SendTabPushNotificationClient> client;

    if (IsMultiProfilePushNotificationHandlingEnabled()) {
      CHECK(profile_);

      client = std::make_unique<SendTabPushNotificationClient>(profile_);
    } else {
      client = std::make_unique<SendTabPushNotificationClient>();
    }

    CHECK_EQ(client->GetClientScope(),
             PushNotificationClientScope::kPerProfile);

    AddPushNotificationClient(std::move(client));

    // Additionally, add Reminder client if STTS reminders are also enabled.
    if (IsSendTabIOSPushNotificationsEnabledWithTabReminders() &&
        IsMultiProfilePushNotificationHandlingEnabled()) {
      CHECK(profile_);

      std::unique_ptr<ReminderNotificationClient> reminder_client =
          std::make_unique<ReminderNotificationClient>(profile_);

      CHECK_EQ(reminder_client->GetClientScope(),
               PushNotificationClientScope::kPerProfile);

      AddPushNotificationClient(std::move(reminder_client));
    }
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
