// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_NOTIFICATION_MODEL_CONTENT_NOTIFICATION_SETTINGS_ACTION_H_
#define IOS_CHROME_BROWSER_CONTENT_NOTIFICATION_MODEL_CONTENT_NOTIFICATION_SETTINGS_ACTION_H_

#import <Foundation/Foundation.h>
#import <UserNotifications/UserNotifications.h>

@interface ContentNotificationSettingsAction : NSObject

// The notifications' OS authorization status. Nil when no change detected.
@property(nonatomic, assign) UNAuthorizationStatus currentAuthorizationStatus;
// The notifications' OS previous authorization status. Nil when no change
// detected.
@property(nonatomic, assign) UNAuthorizationStatus previousAuthorizationStatus;
// Whether content notifications are enabled or disabled.
@property(nonatomic, strong) NSNumber* contentNotificationEnabled;
// Whether sports notifications are enabled or disabled. nil if not set.
@property(nonatomic, strong) NSNumber* sportsNotificationEnabled;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_NOTIFICATION_MODEL_CONTENT_NOTIFICATION_SETTINGS_ACTION_H_
