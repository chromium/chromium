// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_TOGGLE_CELL_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_TOGGLE_CELL_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/home_customization/utils/home_customization_constants.h"

@protocol HomeCustomizationMutator;

// A cell in the customization menu that allows users to toggle the visibility
// of a module using a switch.
@interface HomeCustomizationToggleCell : UICollectionViewCell

// Mutator for communicating with the HomeCustomizationMediator.
@property(nonatomic, weak) id<HomeCustomizationMutator> mutator;

// Configures the cell's properties for a given toggle type.
- (void)configureCellWithType:(CustomizationToggleType)type
                      enabled:(BOOL)enabled;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_TOGGLE_CELL_H_
