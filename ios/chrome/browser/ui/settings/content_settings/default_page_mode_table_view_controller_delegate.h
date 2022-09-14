// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CONTENT_SETTINGS_DEFAULT_PAGE_MODE_TABLE_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CONTENT_SETTINGS_DEFAULT_PAGE_MODE_TABLE_VIEW_CONTROLLER_DELEGATE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/settings/content_settings/default_page_mode.h"

// Delegate for the  screen allowing the user to choose the default mode
// (Desktop/Mobile) for loading pages.
@protocol DefaultPageModeTableViewControllerDelegate

// Called when the user chose a default page mode.
- (void)didSelectMode:(DefaultPageMode)selectedMode;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CONTENT_SETTINGS_DEFAULT_PAGE_MODE_TABLE_VIEW_CONTROLLER_DELEGATE_H_
