// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_VIEW_CONTROLLER_PROTOCOL_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_VIEW_CONTROLLER_PROTOCOL_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/home_customization/utils/home_customization_constants.h"

// Protocol for view controllers within the customization menu.
@protocol HomeCustomizationViewControllerProtocol

// The collection view for the view controller.
@property(nonatomic, strong) UICollectionView* collectionView;

// The diffable data source for the collection view.
@property(nonatomic, strong)
    UICollectionViewDiffableDataSource<CustomizationSection*, NSNumber*>*
        diffableDataSource;

@property(nonatomic, assign) CustomizationMenuPage page;

// Dismisses the presenting view controller.
- (void)dismissCustomizationMenuPage;

// Returns the section for a given `sectionIndex`.
- (NSCollectionLayoutSection*)
      sectionForIndex:(NSInteger)sectionIndex
    layoutEnvironment:(id<NSCollectionLayoutEnvironment>)layoutEnvironment;

// Returns a configured cell at an index path of the collection view.
- (UICollectionViewCell*)configuredCellForIndexPath:(NSIndexPath*)indexPath
                                     itemIdentifier:(NSNumber*)itemIdentifier;

@optional

// Returns a configured header at an index path of the collection view.
- (UICollectionViewCell*)configuredHeaderForIndexPath:(NSIndexPath*)indexPath;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_VIEW_CONTROLLER_PROTOCOL_H_
