// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_BEST_FEATURES_UI_FEATURE_HIGHLIGHT_SCREENSHOT_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_BEST_FEATURES_UI_FEATURE_HIGHLIGHT_SCREENSHOT_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/first_run/ui_bundled/best_features/ui/best_features_item.h"

@protocol ConfirmationAlertActionHandler;

// View for displaying a BestFeaturesItem.
@interface FeatureHighlightScreenshotViewController
    : UIViewController <UINavigationControllerDelegate>

// Creates the view with `BestFeaturesItem` and `actionHandler`.
- (instancetype)initWithFeatureHighlightItem:(BestFeaturesItem*)bestFeaturesItem
                               actionHandler:
                                   (id<ConfirmationAlertActionHandler>)
                                       actionHandler;

@end
#endif  // IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_BEST_FEATURES_UI_FEATURE_HIGHLIGHT_SCREENSHOT_VIEW_CONTROLLER_H_
