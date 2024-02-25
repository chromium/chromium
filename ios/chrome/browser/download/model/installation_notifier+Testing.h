// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_MODEL_INSTALLATION_NOTIFIER_TESTING_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_MODEL_INSTALLATION_NOTIFIER_TESTING_H_

#import "net/base/backoff_entry.h"

// Testing category exposing private methods of InstallationNotifier for
// testing.
@interface InstallationNotifier (Testing)

// Sets the dispatcher.
- (void)setDispatcher:(id<DispatcherProtocol>)dispatcher;

// Resets the dispatcher.
- (void)resetDispatcher;

// Dispatches a block with an exponentially increasing delay.
- (void)dispatchInstallationNotifierBlock;

// Registers for a notification and gives the option to not immediately start
// polling.
- (void)registerForInstallationNotifications:(id)observer
                                withSelector:(SEL)notificationSelector
                                   forScheme:(NSString*)scheme
                                startPolling:(BOOL)poll;

- (net::BackoffEntry::Policy const*)backOffPolicy;

@end

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MODEL_INSTALLATION_NOTIFIER_TESTING_H_
