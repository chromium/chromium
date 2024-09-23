// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_NOTIFICATION_MODEL_CONTENT_NOTIFICATION_NAU_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_CONTENT_NOTIFICATION_MODEL_CONTENT_NOTIFICATION_NAU_CONFIGURATION_H_

#import <Foundation/Foundation.h>

@class UNNotification;
@class UNNotificationContent;

typedef NS_ENUM(NSInteger, NAUActionType);

@class ContentNotificationSettingsAction;

typedef void (^CompletionBlock)(BOOL success);

// This class is intended to store the information needed to send a Notification
// Action Upload for Content Notifications.
@interface ContentNotificationNAUConfiguration : NSObject

// The notification involved in the interaction.
@property(nonatomic, strong) UNNotification* notification;

// The action that happened on the notification.
@property(nonatomic) NAUActionType actionType;

// The Notification content object.
@property(nonatomic, strong) UNNotificationContent* content;

// The settings action that happened on the notification. If it has a value, only a settings
// action is sent.
// TODO(324442228): Refactor settingsAction outside of this configuration.
@property(nonatomic, strong) ContentNotificationSettingsAction* settingsAction;

// Whether the NAU has successfully been sent.
@property(nonatomic, copy) CompletionBlock completion;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_NOTIFICATION_MODEL_CONTENT_NOTIFICATION_NAU_CONFIGURATION_H_
