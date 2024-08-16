// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SHARING_ACTIVITY_SERVICES_ACTIVITIES_REQUEST_DESKTOP_OR_MOBILE_SITE_ACTIVITY_H_
#define IOS_CHROME_BROWSER_UI_SHARING_ACTIVITY_SERVICES_ACTIVITIES_REQUEST_DESKTOP_OR_MOBILE_SITE_ACTIVITY_H_

#import <UIKit/UIKit.h>

#include "ios/web/common/user_agent.h"

@protocol HelpCommands;
class WebNavigationBrowserAgent;

// Activity to request the Desktop or Mobile version of the page.
@interface RequestDesktopOrMobileSiteActivity : UIActivity

// Initializes an activity to change between Mobile versus Desktop user agent,
// with the current `userAgent` and `helpHandler` to execute the action.
- (instancetype)initWithUserAgent:(web::UserAgentType)userAgent
                      helpHandler:(id<HelpCommands>)helpHandler
                  navigationAgent:(WebNavigationBrowserAgent*)agent
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SHARING_ACTIVITY_SERVICES_ACTIVITIES_REQUEST_DESKTOP_OR_MOBILE_SITE_ACTIVITY_H_
