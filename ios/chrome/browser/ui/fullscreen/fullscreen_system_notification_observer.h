// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_SYSTEM_NOTIFICATION_OBSERVER_H_
#define IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_SYSTEM_NOTIFICATION_OBSERVER_H_

#import <UIKit/UIKit.h>

class FullscreenController;
class FullscreenMediator;

// Helper class that listens for system notifications.  This class will disable
// fullscreen when:
// - voice over is enabled
// - the keyboard is visible
// Additionally, this object notifies the mediator of foreground events.
@interface FullscreenSystemNotificationObserver : NSObject

// Designated initializer that updates `controller` and `mediator` for system
// notifications.
- (nullable instancetype)
initWithController:(nonnull FullscreenController*)controller
          mediator:(nonnull FullscreenMediator*)mediator
    NS_DESIGNATED_INITIALIZER;
- (nullable instancetype)init NS_UNAVAILABLE;

// Stops observing notifications.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_SYSTEM_NOTIFICATION_OBSERVER_H_
