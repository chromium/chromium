// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_MODEL_INSTALLATION_NOTIFIER_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_MODEL_INSTALLATION_NOTIFIER_H_

#import <Foundation/Foundation.h>
#include <stdint.h>

// Protocol used to mock the delayed dispatching for the unit tests.
// Calls `block` after `delayInNSec`.
@protocol DispatcherProtocol <NSObject>
- (void)dispatchAfter:(int64_t)delayInNSec withBlock:(dispatch_block_t)block;
@end

@interface InstallationNotifier : NSObject
// Returns singleton instance.
+ (InstallationNotifier*)sharedInstance;

// Registers `observer` to be sent a notification named `scheme` when a URL
// with `scheme` can be opened. `observer` must not be nil. If `scheme` is nil
// or an empty string, `observer` is not registered for anything.
- (void)registerForInstallationNotifications:(id)observer
                                withSelector:(SEL)notificationSelector
                                   forScheme:(NSString*)scheme;

// Unregisters all the NSNotifications ever asked by `observer`.
- (void)unregisterForNotifications:(id)observer;

// Performs a check for installed apps right away and restarts the polling.
// There is usually no need for registered observers to call this method, unless
// registered observers need to know the accurate state of installed native
// apps.
- (void)checkNow;

// Stops any queued polling.
- (void)stopPolling;
@end

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MODEL_INSTALLATION_NOTIFIER_H_
