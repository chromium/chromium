// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/push_notification/push_notification_service.h"

#import "ios/chrome/browser/push_notification/push_notification_client_manager.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

PushNotificationService::PushNotificationService()
    : client_manager_(std::make_unique<PushNotificationClientManager>()) {}

PushNotificationService::~PushNotificationService() = default;

PushNotificationClientManager*
PushNotificationService::GetPushNotificationClientManager() {
  return client_manager_.get();
}
