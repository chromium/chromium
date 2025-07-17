// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_BWG_UI_BWG_SETTINGS_CONSUMER_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_BWG_UI_BWG_SETTINGS_CONSUMER_H_

#import <UIKit/UIKit.h>

// Consumer protocol for BWG settings.
@protocol BWGSettingsConsumer

// Sets the Precise Location boolean.
- (void)setPreciseLocationEnabled:(BOOL)enabled;

// Sets the Page Content Sharing boolean.
- (void)setPageContentSharingEnabled:(BOOL)enabled;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_BWG_UI_BWG_SETTINGS_CONSUMER_H_
