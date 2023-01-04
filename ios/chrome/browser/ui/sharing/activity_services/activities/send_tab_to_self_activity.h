// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SHARING_ACTIVITY_SERVICES_ACTIVITIES_SEND_TAB_TO_SELF_ACTIVITY_H_
#define IOS_CHROME_BROWSER_UI_SHARING_ACTIVITY_SERVICES_ACTIVITIES_SEND_TAB_TO_SELF_ACTIVITY_H_

#import <UIKit/UIKit.h>

@protocol BrowserCoordinatorCommands;
@class ShareToData;

// Activity that sends the tab to another of the user's devices.
@interface SendTabToSelfActivity : UIActivity

// Initializes the send tab to self activity with the given `data` and the
// `handler` that is used to add the tab to the other device.
- (instancetype)initWithData:(ShareToData*)data
                     handler:(id<BrowserCoordinatorCommands>)handler
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SHARING_ACTIVITY_SERVICES_ACTIVITIES_SEND_TAB_TO_SELF_ACTIVITY_H_
