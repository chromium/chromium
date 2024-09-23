// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/push_notification_service_extension/push_notification_service_extension.h"

@implementation PushNotificationServiceExtension

- (instancetype)init {
  self = [super init];
  return self;
}

- (void)didReceiveNotificationRequest:(UNNotificationRequest*)request
                   withContentHandler:
                       (void (^)(UNNotificationContent* contentToDeliver))
                           contentHandler {
  contentHandler(request.content);
}

@end