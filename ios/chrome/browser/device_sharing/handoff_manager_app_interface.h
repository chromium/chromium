// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEVICE_SHARING_HANDOFF_MANAGER_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_DEVICE_SHARING_HANDOFF_MANAGER_APP_INTERFACE_H_

#import <UIKit/UIKit.h>

// The app interface for handoff tests.
@interface HandoffManagerAppInterface : NSObject

// Current user activity web page url from the handoff manager.
+ (NSURL*)currentUserActivityWebPageURL;

@end

#endif  // IOS_CHROME_BROWSER_DEVICE_SHARING_HANDOFF_MANAGER_APP_INTERFACE_H_
