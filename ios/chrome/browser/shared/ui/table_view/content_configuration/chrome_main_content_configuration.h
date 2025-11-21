// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CONTENT_CONFIGURATION_CHROME_MAIN_CONTENT_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CONTENT_CONFIGURATION_CHROME_MAIN_CONTENT_CONFIGURATION_H_

#import <Foundation/Foundation.h>

// The class for the main content configurations (i.e. directly used in cells).
@protocol ChromeMainContentConfiguration <UIContentConfiguration>

// Sets whether the content configuration is displayed in a view that has an
// accessory view.
- (void)setHasAccessoryView:(BOOL)hasAccessoryView;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CONTENT_CONFIGURATION_CHROME_MAIN_CONTENT_CONFIGURATION_H_
