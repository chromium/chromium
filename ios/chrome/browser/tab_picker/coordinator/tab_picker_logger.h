// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_PICKER_COORDINATOR_TAB_PICKER_LOGGER_H_
#define IOS_CHROME_BROWSER_TAB_PICKER_COORDINATOR_TAB_PICKER_LOGGER_H_

#import <Foundation/Foundation.h>

#import "ios/web/public/web_state.h"

// Protocol for logging events from the TabPicker component.
@protocol TabPickerLogger <NSObject>

@optional

// Logs that the tab picker was shown.
- (void)logTabPickerShown;

// Logs that the tab picker was hidden.
- (void)logTabPickerHidden;

// Logs that a tab will be loaded.
- (void)logWillLoadTabWithTitle:(NSString*)title tabID:(web::WebStateID)tabID;

// Logs that a tab finished loading.
- (void)logDidLoadTabWithSuccess:(BOOL)success
                           title:(NSString*)title
                           tabID:(web::WebStateID)tabID;

// Logs that a tab will be forced to realize.
- (void)logWillRealizeTabWithTitle:(NSString*)title
                             tabID:(web::WebStateID)tabID;

// Logs that a tab was forced to realize.
- (void)logDidRealizeTabWithTitle:(NSString*)title tabID:(web::WebStateID)tabID;

@end

#endif  // IOS_CHROME_BROWSER_TAB_PICKER_COORDINATOR_TAB_PICKER_LOGGER_H_
