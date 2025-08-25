// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_BACKGROUND_PICKER_ACTION_SHEET_COORDINATOR_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_BACKGROUND_PICKER_ACTION_SHEET_COORDINATOR_H_

@protocol HomeCustomizationSearchEngineLogoMediatorProvider;
@protocol HomeCustomizationBackgroundPickerPresentationDelegate;

#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"

// Coordinator responsible for displaying an action sheet to pick a background
// customization option, such as selecting a background color or gallery image.
// Presents new view controllers modally based on user selection.
@interface HomeCustomizationBackgroundPickerActionSheetCoordinator
    : ActionSheetCoordinator

// Initializes the background picker action sheet coordinator.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                sourceView:(UIView*)sourceView;

// A provider responsible for supplying a logo vendor object.
@property(nonatomic, weak) id<HomeCustomizationSearchEngineLogoMediatorProvider>
    searchEngineLogoMediatorProvider;

// Delegate to inform about presentation events.
@property(nonatomic, weak)
    id<HomeCustomizationBackgroundPickerPresentationDelegate>
        presentationDelegate;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_BACKGROUND_PICKER_ACTION_SHEET_COORDINATOR_H_
