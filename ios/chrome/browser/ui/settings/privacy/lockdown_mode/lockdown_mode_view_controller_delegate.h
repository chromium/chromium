// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_LOCKDOWN_MODE_LOCKDOWN_MODE_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_LOCKDOWN_MODE_LOCKDOWN_MODE_VIEW_CONTROLLER_DELEGATE_H_

#import <UIKit/UIKit.h>

// Delegate for communicating actions that happen in the view controller. This
// is usually used to relay information to the model.
@protocol LockdownModeViewControllerDelegate

// Sends switch toggle response to the model so that it can be updated.
- (void)didEnableBrowserLockdownMode:(BOOL)enabled;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_LOCKDOWN_MODE_LOCKDOWN_MODE_VIEW_CONTROLLER_DELEGATE_H_
