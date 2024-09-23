// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_COLLECTION_CONFIGURATOR_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_COLLECTION_CONFIGURATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/home_customization/utils/home_customization_constants.h"

@protocol HomeCustomizationViewControllerProtocol;

// Configures collection views within the Home customization menu.
@interface HomeCustomizationCollectionConfigurator : NSObject

// Initializes the configurator for a given `page` in the menu.
- (instancetype)initWithViewController:
    (UIViewController<HomeCustomizationViewControllerProtocol,
                      UICollectionViewDelegate>*)viewController
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Configures the collection view with a diffable data source on the view
// controller.
- (void)configureCollectionView;

// Configures the navigation bar for view controller.
- (void)configureNavigationBar;

// Returns a section representing a vertical list of cells.
- (NSCollectionLayoutSection*)verticalListSectionForLayoutEnvironment:
    (id<NSCollectionLayoutEnvironment>)layoutEnvironment;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_COLLECTION_CONFIGURATOR_H_
