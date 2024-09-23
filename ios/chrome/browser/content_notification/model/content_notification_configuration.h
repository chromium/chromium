// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_NOTIFICATION_MODEL_CONTENT_NOTIFICATION_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_CONTENT_NOTIFICATION_MODEL_CONTENT_NOTIFICATION_CONFIGURATION_H_

#import <Foundation/Foundation.h>

class AuthenticationService;
@protocol SingleSignOnService;

// Configuration object used by the ContentNotificationService.
@interface ContentNotificationConfiguration : NSObject

// AuthenticationService used by ContentNotificationService.
@property(nonatomic, assign) AuthenticationService* authService;

// The SingleSignOnService used by ContentNotificationService.
@property(nonatomic, strong) id<SingleSignOnService> ssoService;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_NOTIFICATION_MODEL_CONTENT_NOTIFICATION_CONFIGURATION_H_
