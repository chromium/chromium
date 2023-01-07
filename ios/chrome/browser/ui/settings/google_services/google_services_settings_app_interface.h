// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_GOOGLE_SERVICES_SETTINGS_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_GOOGLE_SERVICES_SETTINGS_APP_INTERFACE_H_

#import <UIKit/UIKit.h>

// The app interface for Google services settings tests.
@interface GoogleServicesSettingsAppInterface : NSObject

// Blocks all navigation requests to be loaded in Chrome.
+ (void)blockAllNavigationRequestsForCurrentWebState;

// Unblocks all navigation requests to be loaded in Chrome.
+ (void)unblockAllNavigationRequestsForCurrentWebState;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_GOOGLE_SERVICES_SETTINGS_APP_INTERFACE_H_
