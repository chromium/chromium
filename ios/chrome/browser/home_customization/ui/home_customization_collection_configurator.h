// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_COLLECTION_CONFIGURATOR_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_COLLECTION_CONFIGURATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/home_customization/utils/home_customization_constants.h"

// Configures collection views within the Home customization menu.
@interface HomeCustomizationCollectionConfigurator : NSObject

// Initializes the configurator for a given `page` in the menu.
- (instancetype)initWithPage:(CustomizationMenuPage)page
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// The diffable data source for the collection view.
@property(nonatomic, weak)
    UICollectionViewDiffableDataSource* diffableDataSource;

// Returns a collection view layout suited for the current page..
- (UICollectionViewLayout*)collectionViewLayout;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_COLLECTION_CONFIGURATOR_H_
