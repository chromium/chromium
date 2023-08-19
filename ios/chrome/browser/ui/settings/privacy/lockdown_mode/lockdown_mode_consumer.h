// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_LOCKDOWN_MODE_LOCKDOWN_MODE_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_LOCKDOWN_MODE_LOCKDOWN_MODE_CONSUMER_H_

#import <UIKit/UIKit.h>

@protocol LockdownModeConsumer

// Reload cells for items. Does nothing if the model is not loaded yet.
- (void)reloadCellsForItems;

// Sets Browser lockdown mode enabled.
- (void)setBrowserLockdownModeEnabled:(BOOL)enabled;

// Sets OS lockdown mode enabled.
- (void)setOSLockdownModeEnabled:(BOOL)enabled;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_LOCKDOWN_MODE_LOCKDOWN_MODE_CONSUMER_H_
