// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_LAYOUT_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_LAYOUT_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_paging.h"

@class GridItemIdentifier;

enum class TabsSectionHeaderType {
  kNone,          // No header is shown.
  kSearch,        // The Search header is shown, with the number of matches.
  kInactiveTabs,  // The Inactive Tabs button, or the Inactive Tabs preamble is
                  // shown.
  kAnimatingOut,  // The previous header is being animated out. This adds a
                  // 0.1pt high empty header.
                  // TODO(crbug.com/40944664): Remove once the button is a cell
                  // and not a header.
  kTabGroup,      // Tab Group information header is shown.
};

// A collection view compositional layout that displays items in a grid.
//
// - The number of columns adapts to the available width and whether an
//     Accessibility Dynamic Type is selected.
// - Items have a 4/3 aspect ratio in portrait, and the same aspect ratio as the
//   screen in landscape.
// - Item insertions and deletions are animated by default.
@interface GridLayout : UICollectionViewCompositionalLayout

// Whether to animate item insertions and deletions. Defaults to YES.
@property(nonatomic, assign) BOOL animatesItemUpdates;

// The type of header (if any) to account for in the Tabs section. Defaults to
// kNone.
@property(nonatomic, assign) TabsSectionHeaderType tabsSectionHeaderType;

// The insets to add to the different sections. Defaults to UIEdgeInsetsZero.
@property(nonatomic, assign) NSDirectionalEdgeInsets sectionInsets;

// The diffable data source used to configure the layout. It is used
// to resolve section indices.
@property(nonatomic, weak)
    UICollectionViewDiffableDataSource<NSString*, GridItemIdentifier*>*
        diffableDataSource;

- (instancetype)init NS_DESIGNATED_INITIALIZER;

#pragma mark - Unavailable initializers

- (instancetype)initWithSection:(NSCollectionLayoutSection*)section
    NS_UNAVAILABLE;
- (instancetype)initWithSection:(NSCollectionLayoutSection*)section
                  configuration:
                      (UICollectionViewCompositionalLayoutConfiguration*)
                          configuration NS_UNAVAILABLE;
- (instancetype)initWithSectionProvider:
    (UICollectionViewCompositionalLayoutSectionProvider)sectionProvider
    NS_UNAVAILABLE;
- (instancetype)
    initWithSectionProvider:
        (UICollectionViewCompositionalLayoutSectionProvider)sectionProvider
              configuration:(UICollectionViewCompositionalLayoutConfiguration*)
                                configuration NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_LAYOUT_H_
