// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_PICKER_CELL_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_PICKER_CELL_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/home_customization/ui/home_customization_background_cell.h"

// Mutator protocol for the UI layer to communicate to the
// HomeCustomizationMediator.
@protocol HomeCustomizationMutator;
@protocol HomeCustomizationBackgroundPickerPresentationDelegate;

// Represents a cell with a picker button that allows users to choose more
// background options for the NTP customization. This cell enables users to
// explore additional backgrounds beyond the initial selection, providing
// an easy way to expand their choices.
@interface HomeCustomizationBackgroundPickerCell
    : HomeCustomizationBackgroundCell

// The delegate object that will handle showing the background picker alert
// sheet.
@property(nonatomic, weak)
    id<HomeCustomizationBackgroundPickerPresentationDelegate>
        delegate;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_PICKER_CELL_H_
