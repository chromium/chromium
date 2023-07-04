// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/utils/notification_observer_bridge.h"

#import "base/check.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation NotificationObserverBridge {
  base::RepeatingCallback<void(NSNotification*)> _callback;
}

- (instancetype)initForNotification:(NSNotificationName)notification
                      usingCallback:(NotificationCallback)callback {
  self = [super init];
  if (self) {
    CHECK(callback);
    _callback = callback;
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(didReceiveNotification:)
               name:notification
             object:nil];
  }
  return self;
}

- (void)didReceiveNotification:(NSNotification*)notification {
  _callback.Run(notification);
}

@end
