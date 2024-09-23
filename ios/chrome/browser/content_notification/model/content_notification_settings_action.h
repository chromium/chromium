// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_NOTIFICATION_MODEL_CONTENT_NOTIFICATION_SETTINGS_ACTION_H_
#define IOS_CHROME_BROWSER_CONTENT_NOTIFICATION_MODEL_CONTENT_NOTIFICATION_SETTINGS_ACTION_H_

#import <Foundation/Foundation.h>
#import <UserNotifications/UserNotifications.h>

typedef NS_ENUM(NSInteger, SettingsToggleType);

@interface ContentNotificationSettingsAction : NSObject

// The notifications' OS authorization status. Nil when no change detected.
@property(nonatomic, assign) UNAuthorizationStatus currentAuthorizationStatus;
// The notifications' OS previous authorization status. Nil when no change
// detected.
@property(nonatomic, assign) UNAuthorizationStatus previousAuthorizationStatus;
// Which toggle has changed status. 0 if None.
@property(nonatomic) SettingsToggleType toggleChanged;
// Whether the `toggleChanged` is enabled or disabled.
@property(nonatomic, assign) BOOL toggleStatus;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_NOTIFICATION_MODEL_CONTENT_NOTIFICATION_SETTINGS_ACTION_H_
