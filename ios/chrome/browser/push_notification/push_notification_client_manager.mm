// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/push_notification/push_notification_client_manager.h"

#import <Foundation/Foundation.h>
#import <vector>

#import "components/optimization_guide/core/optimization_guide_features.h"
#import "ios/chrome/browser/commerce/push_notification/commerce_push_notification_client.h"
#import "ios/chrome/browser/commerce/push_notification/push_notification_feature.h"
#import "ios/chrome/browser/push_notification/push_notification_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

PushNotificationClientManager::PushNotificationClientManager() {
  if (IsPriceNotificationsEnabled() &&
      optimization_guide::features::IsPushNotificationsEnabled()) {
    AddPushNotificationClient(
        std::make_unique<CommercePushNotificationClient>());
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
  for (auto& client : clients_) {
    client.second->HandleNotificationInteraction(notification_response);
  }
}

UIBackgroundFetchResult
PushNotificationClientManager::HandleNotificationReception(
    NSDictionary<NSString*, id>* user_info) {
  UIBackgroundFetchResult result = UIBackgroundFetchResultNoData;
  for (auto& client : clients_) {
    UIBackgroundFetchResult client_result =
        client.second->HandleNotificationReception(user_info);
    if (client_result == UIBackgroundFetchResultNewData) {
      return UIBackgroundFetchResultNewData;
    } else if (client_result == UIBackgroundFetchResultFailed) {
      result = client_result;
    }
  }

  return result;
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
  return {PushNotificationClientId::kCommerce};
}

void PushNotificationClientManager::OnBrowserReady() {
  for (auto& client : clients_) {
    client.second->OnBrowserReady();
  }
}

std::string PushNotificationClientManager::PushNotificationClientIdToString(
    PushNotificationClientId client_id) {
  switch (client_id) {
    case PushNotificationClientId::kCommerce: {
      return "PRICE_DROP";
    }
  }
}
