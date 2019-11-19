// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_ACTIVITIES_SEND_TAB_TO_SELF_ACTIVITY_H_
#define IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_ACTIVITIES_SEND_TAB_TO_SELF_ACTIVITY_H_

#import <UIKit/UIKit.h>

@protocol BrowserCommands;

// Activity that sends the tab to another of the user's devices.
@interface SendTabToSelfActivity : UIActivity

// Identifier for the send tab to self activity.
+ (NSString*)activityIdentifier;

// Initialize the send tab to self activity with the |dispatcher| that is used
// to add the tab to the other device.
- (instancetype)initWithDispatcher:(id<BrowserCommands>)dispatcher;

@end

#endif  // IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_ACTIVITIES_SEND_TAB_TO_SELF_ACTIVITY_H_