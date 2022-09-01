// Copyright 2022 The Chromium Authors. All rights reserved.
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

void PushNotificationClientManager::OnNotificationInteraction(
    UNNotificationResponse* notification_response) {
  NSDictionary* notification =
      notification_response.notification.request.content.userInfo;

  PushNotificationClientId client_id =
      static_cast<PushNotificationClientId>([[notification
          objectForKey:kClientIdPushNotificationDictionaryKey] integerValue]);

  auto it = clients_.find(client_id);
  if (it != clients_.end()) {
    it->second->HandleInteractedNotification(notification_response);
  }
}

void PushNotificationClientManager::OnNotificationReceived(
    NSDictionary<NSString*, id>* notification) {
  PushNotificationClientId client_id =
      static_cast<PushNotificationClientId>([[notification
          objectForKey:kClientIdPushNotificationDictionaryKey] integerValue]);

  auto it = clients_.find(client_id);
  if (it != clients_.end()) {
    it->second->HandleReceivedNotification(notification);
  }
}
