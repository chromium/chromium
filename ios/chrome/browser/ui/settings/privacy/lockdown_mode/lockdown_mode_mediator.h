// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_LOCKDOWN_MODE_LOCKDOWN_MODE_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_LOCKDOWN_MODE_LOCKDOWN_MODE_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/settings/privacy/lockdown_mode/lockdown_mode_consumer.h"
#import "ios/chrome/browser/ui/settings/privacy/lockdown_mode/lockdown_mode_view_controller_delegate.h"

// Mediator for the Lockdown Mode Settings UI.
@interface LockdownModeMediator : NSObject <LockdownModeViewControllerDelegate>

// Consumer for mediator.
@property(nonatomic, weak) id<LockdownModeConsumer> consumer;

// Cleans up anything before mediator shuts down.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_LOCKDOWN_MODE_LOCKDOWN_MODE_MEDIATOR_H_
