// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CONTENT_CONFIGURATION_CHROME_CONTENT_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CONTENT_CONFIGURATION_CHROME_CONTENT_CONFIGURATION_H_

#import <UIKit/UIKit.h>

@protocol ChromeContentView;

// Protocol to specialize UIContentConfiguration to Chrome needs.
@protocol ChromeContentConfiguration <UIContentConfiguration>

// This is the same method as `makeContentView`, but with a more specific type.
- (UIView<ChromeContentView>*)makeChromeContentView;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CONTENT_CONFIGURATION_CHROME_CONTENT_CONFIGURATION_H_
