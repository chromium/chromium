// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_CELL_SUBCLASSING_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_CELL_SUBCLASSING_H_

#import "ios/chrome/browser/home_customization/ui/home_customization_background_cell.h"

@interface HomeCustomizationBackgroundCell (Subclassing)

// Container view that provides the outer highlight border.
// Acts as a decorative wrapper for the inner content.
@property(nonatomic, strong) UIView* borderWrapperView;

// Main content view rendered inside the border wrapper.
// Displays the core visual element.
@property(nonatomic, strong) UIStackView* innerContentView;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_CELL_SUBCLASSING_H_
