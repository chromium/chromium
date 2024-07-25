// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_LINK_CELL_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_LINK_CELL_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/home_customization/utils/home_customization_constants.h"

@protocol HomeCustomizationMutator;

// A cell in the customization menu that allows users to navigate to an external
// URL.
@interface HomeCustomizationLinkCell : UICollectionViewCell

// Mutator for handling model updates.
@property(nonatomic, weak) id<HomeCustomizationMutator> mutator;

// Configures the cell's properties for a given link type.
- (void)configureCellWithType:(CustomizationLinkType)type;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_LINK_CELL_H_
