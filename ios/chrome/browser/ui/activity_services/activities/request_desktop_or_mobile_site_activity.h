// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_ACTIVITIES_REQUEST_DESKTOP_OR_MOBILE_SITE_ACTIVITY_H_
#define IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_ACTIVITIES_REQUEST_DESKTOP_OR_MOBILE_SITE_ACTIVITY_H_

#import <UIKit/UIKit.h>

#include "ios/web/common/user_agent.h"

@protocol BrowserCommands;

// Activity to request the Desktop or Mobile version of the page.
@interface RequestDesktopOrMobileSiteActivity : UIActivity

// Identifier for the activity.
+ (NSString*)activityIdentifier;

- (instancetype)initWithDispatcher:(id<BrowserCommands>)dispatcher
                         userAgent:(web::UserAgentType)userAgent
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_ACTIVITIES_REQUEST_DESKTOP_OR_MOBILE_SITE_ACTIVITY_H_
