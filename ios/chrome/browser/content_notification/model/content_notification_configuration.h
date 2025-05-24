// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_NOTIFICATION_MODEL_CONTENT_NOTIFICATION_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_CONTENT_NOTIFICATION_MODEL_CONTENT_NOTIFICATION_CONFIGURATION_H_

#import <Foundation/Foundation.h>

class ChromeAccountManagerService;
@protocol SingleSignOnService;

namespace signin {
class IdentityManager;
}  // namespace signin

// Configuration object used by the ContentNotificationService.
@interface ContentNotificationConfiguration : NSObject

// IdentityManager used by ContentNotificationService.
@property(nonatomic, assign) signin::IdentityManager* identityManager;

// ChromeAccountManagerService use by ContentNotificationService.
@property(nonatomic, assign) ChromeAccountManagerService* accountManager;

// The SingleSignOnService used by ContentNotificationService.
@property(nonatomic, strong) id<SingleSignOnService> ssoService;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_NOTIFICATION_MODEL_CONTENT_NOTIFICATION_CONFIGURATION_H_
