// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_UTILS_NOTIFICATION_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_UTILS_NOTIFICATION_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#import "base/functional/callback.h"

using NotificationCallback = base::RepeatingCallback<void(NSNotification*)>;

/// This object can be used to observe NSNotification from C++ without needing
/// to unregister the notification.
@interface NotificationObserverBridge : NSObject

/// Init with the `notification` to be observed and call `block` when triggered.
- (instancetype)initForNotification:(NSNotificationName)notification
                      usingCallback:(NotificationCallback)callback
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_UTILS_NOTIFICATION_OBSERVER_BRIDGE_H_
