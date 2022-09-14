// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/push_notification/push_notification_client_manager.h"

#import <Foundation/Foundation.h>

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
NSString* kClientIdPushNotificationDictionaryKey =
    @"push_notification_client_id";
}  // namespace

PushNotificationClientManager::PushNotificationClientManager() = default;

PushNotificationClientManager::~PushNotificationClientManager() = default;

void PushNotificationClientManager::AddPushNotificationClient(
    PushNotificationClientId client_id,
    std::unique_ptr<PushNotificationClient> client) {
  clients_.insert(std::make_pair(client_id, std::move(client)));
}

void PushNotificationClientManager::HandleNotificationInteraction(
    UNNotificationResponse* notification_response) {
  NSDictionary* user_info =
      notification_response.notification.request.content.userInfo;

  PushNotificationClientId client_id =
      static_cast<PushNotificationClientId>([[user_info
          objectForKey:kClientIdPushNotificationDictionaryKey] integerValue]);

  auto it = clients_.find(client_id);
  if (it != clients_.end()) {
    it->second->HandleNotificationInteraction(notification_response);
  }
}

UIBackgroundFetchResult
PushNotificationClientManager::HandleNotificationReception(
    NSDictionary<NSString*, id>* user_info) {
  PushNotificationClientId client_id =
      static_cast<PushNotificationClientId>([[user_info
          objectForKey:kClientIdPushNotificationDictionaryKey] integerValue]);

  auto it = clients_.find(client_id);
  if (it != clients_.end()) {
    return it->second->HandleNotificationReception(user_info);
  }

  return UIBackgroundFetchResultNoData;
}
