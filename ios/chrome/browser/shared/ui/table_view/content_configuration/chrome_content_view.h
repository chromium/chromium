// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CONTENT_CONFIGURATION_CHROME_CONTENT_VIEW_H_
#define IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CONTENT_CONFIGURATION_CHROME_CONTENT_VIEW_H_

#import <UIKit/UIKit.h>

// Protocol to speciliaze UIContentView to Chrome needs.
@protocol ChromeContentView <UIContentView>

// Returns whether this view has a custom accessibility activation point that
// should be used instead of the default.
- (BOOL)hasCustomAccessibilityActivationPoint;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CONTENT_CONFIGURATION_CHROME_CONTENT_VIEW_H_
