// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/push_notification/push_notification_client.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

PushNotificationClient::PushNotificationClient(
    PushNotificationClientId client_id)
    : client_id_(client_id) {}

PushNotificationClient::~PushNotificationClient() = default;

PushNotificationClientId PushNotificationClient::GetClientId() {
  return client_id_;
}