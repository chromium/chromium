// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_WPT_CWT_WEBDRIVER_APP_INTERFACE_H_
#define IOS_CHROME_TEST_WPT_CWT_WEBDRIVER_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

// Methods used by CWTRequestHandler to perform browser-level actions such as
// opening and closing tabs, navigating to a URL, and injecting JavaScript.
// These methods run on a background thread in the app process in order to
// avoid deadlock while waiting for actions to complete on the main thread.
@interface CWTWebDriverAppInterface : NSObject

// Loads the given URL in the tab identified by |tabID|. Returns an error if the
// page fails to load within |timeout| seconds or if no such tab exists.
+ (NSError*)loadURL:(NSString*)URL
               inTab:(NSString*)tabID
    timeoutInSeconds:(NSTimeInterval)timeout;

// Returns the id of the current tab. If no tabs are open, returns nil.
+ (NSString*)currentTabID;

// Returns an array containing the ids of all open tabs.
+ (NSArray*)tabIDs;

// Closes the tab identified by |tabID|. Returns an error if there is no such
// tab.
+ (NSError*)closeTabWithID:(NSString*)ID;

// Makes the tab identified by |ID| the current tab. Returns an error if there
// is no such tab.
+ (NSError*)switchToTabWithID:(NSString*)ID;

// Executes the given JavaScript function in the tab identified by |tabID|. This
// must be a function that takes a single argument, and uses this argument as a
// completion handler. Returns the value passed to the completion handler. If
// no such tab exists, or if script execution does not complete within |timeout|
// seconds, returns nil.
+ (NSString*)executeAsyncJavaScriptFunction:(NSString*)function
                                      inTab:(NSString*)tabID
                           timeoutInSeconds:(NSTimeInterval)timeout;

// Allows script to open tabs using "window.open" JavaScript calls.
+ (void)enablePopups;

// Takes a snapshot of the specified tab. Returns the snapshot as a base64-
// encoded image. If no such tab exists, returns nil.
+ (NSString*)takeSnapshotOfTabWithID:(NSString*)ID;

@end

#endif  // IOS_CHROME_TEST_WPT_CWT_WEBDRIVER_APP_INTERFACE_H_
