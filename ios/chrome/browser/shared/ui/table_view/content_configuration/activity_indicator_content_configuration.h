// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CONTENT_CONFIGURATION_ACTIVITY_INDICATOR_CONTENT_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CONTENT_CONFIGURATION_ACTIVITY_INDICATOR_CONTENT_CONFIGURATION_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/content_configuration/chrome_content_configuration.h"

// Configuration object for a TableView cell holding an activity indicator.
// It is using a ActivityIndicatorCellContentView as content view.
@interface ActivityIndicatorContentConfiguration
    : NSObject <ChromeContentConfiguration>

// The updates to properties must be reflected in the copy method.
// LINT.IfChange(Copy)

// The style of the activity indicator. Default is
// UIActivityIndicatorViewStyleMedium.
@property(nonatomic, assign) UIActivityIndicatorViewStyle style;

// The color of the activity indicator.
@property(nonatomic, strong) UIColor* color;

// Whether the activity indicator should be animating. Default is YES.
@property(nonatomic, assign) BOOL animating;

// LINT.ThenChange(activity_indicator_content_configuration.mm:Copy)

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CONTENT_CONFIGURATION_ACTIVITY_INDICATOR_CONTENT_CONFIGURATION_H_
