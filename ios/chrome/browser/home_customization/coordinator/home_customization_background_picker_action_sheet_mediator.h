// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_BACKGROUND_PICKER_ACTION_SHEET_MEDIATOR_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_BACKGROUND_PICKER_ACTION_SHEET_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/home_customization/ui/home_customization_background_picker_action_sheet_mutator.h"

class HomeBackgroundCustomizationService;

// Mediator responsible for managing the background customization action sheet,
// which allows the user to pick a background option.
@interface HomeCustomizationBackgroundPickerActionSheetMediator
    : NSObject <HomeCustomizationBackgroundPickerActionSheetMutator>

// Initializes the mediator with the provided home background customization
// service.
- (instancetype)initWithHomeBackgroundCustomizationService:
    (HomeBackgroundCustomizationService*)homeBackgroundCustomizationService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end
#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_BACKGROUND_PICKER_ACTION_SHEET_MEDIATOR_H_
