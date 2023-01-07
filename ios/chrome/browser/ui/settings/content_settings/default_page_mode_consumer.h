// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CONTENT_SETTINGS_DEFAULT_PAGE_MODE_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CONTENT_SETTINGS_DEFAULT_PAGE_MODE_CONSUMER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/settings/content_settings/default_page_mode.h"

// Consumer protocol for the screen allowing the user to choose the default mode
// (Desktop/Mobile) for loading pages.
@protocol DefaultPageModeConsumer

// Sets the mode in which pages are loaded by default.
- (void)setDefaultPageMode:(DefaultPageMode)mode;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CONTENT_SETTINGS_DEFAULT_PAGE_MODE_CONSUMER_H_
