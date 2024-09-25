// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/push_notification/model/push_notification_client_manager.h"

#import <Foundation/Foundation.h>

#import <optional>
#import <vector>

#import "base/feature_list.h"
#import "base/task/sequenced_task_runner.h"
#import "components/optimization_guide/core/optimization_guide_features.h"
#import "components/send_tab_to_self/features.h"
#import "ios/chrome/browser/commerce/model/push_notification/commerce_push_notification_client.h"
#import "ios/chrome/browser/commerce/model/push_notification/push_notification_feature.h"
#import "ios/chrome/browser/content_notification/model/content_notification_client.h"
#import "ios/chrome/browser/push_notification/model/constants.h"
#import "ios/chrome/browser/push_notification/model/push_notification_util.h"
#import "ios/chrome/browser/safety_check_notifications/model/safety_check_notification_client.h"
#import "ios/chrome/browser/send_tab_to_self/model/send_tab_push_notification_client.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/tips_notifications/model/tips_notification_client.h"

PushNotificationClientManager::PushNotificationClientManager() {
  if (IsPriceNotificationsEnabled() &&
      optimization_guide::features::IsPushNotificationsEnabled()) {
    AddPushNotificationClient(
        std::make_unique<CommercePushNotificationClient>());
  }

  if (IsIOSTipsNotificationsEnabled()) {
    AddPushNotificationClient(std::make_unique<TipsNotificationClient>());
  }

  if (IsContentNotificationExperimentEnabled()) {
    AddPushNotificationClient(std::make_unique<ContentNotificationClient>());
  }

  if (IsSafetyCheckNotificationsEnabled()) {
    AddPushNotificationClient(std::make_unique<SafetyCheckNotificationClient>(
        base::SequencedTaskRunner::GetCurrentDefault()));
  }

  if (base::FeatureList::IsEnabled(
          send_tab_to_self::kSendTabToSelfIOSPushNotifications)) {
    AddPushNotificationClient(
        std::make_unique<SendTabPushNotificationClient>());
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
    clients_[clientId.value()]->HandleNotificationInteraction(
        notification_response);
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
    client_result =
        clients_[clientId.value()]->HandleNotificationReception(user_info);
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
      PushNotificationClientId::kCommerce};
  if (IsContentNotificationExperimentEnabled()) {
    client_ids.push_back(PushNotificationClientId::kContent);
    client_ids.push_back(PushNotificationClientId::kSports);
  }
  if (IsIOSTipsNotificationsEnabled()) {
    client_ids.push_back(PushNotificationClientId::kTips);
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

std::string PushNotificationClientManager::PushNotificationClientIdToString(
    PushNotificationClientId client_id) {
  switch (client_id) {
    case PushNotificationClientId::kCommerce: {
      return kCommerceNotificationKey;
    }
    case PushNotificationClientId::kContent: {
      return kContentNotificationKey;
    }
    case PushNotificationClientId::kTips: {
      return kTipsNotificationKey;
    }
    case PushNotificationClientId::kSports: {
      return kSportsNotificationKey;
    }
    case PushNotificationClientId::kSafetyCheck: {
      return kSafetyCheckNotificationKey;
    }
    case PushNotificationClientId::kSendTab: {
      return kSendTabNotificationKey;
    }
  }
}
