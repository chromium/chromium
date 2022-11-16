// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_PINNED_TABS_PINNED_CELL_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_PINNED_TABS_PINNED_CELL_H_

#import <UIKit/UIKit.h>

// A cell for the pinned tabs view. Contains an icon, title, snapshot.
@interface PinnedCell : UICollectionViewCell

// Unique identifier for the cell's contents. This is used to ensure that
// updates in an asynchronous callback are only made if the item is the same.
@property(nonatomic, copy) NSString* itemIdentifier;
// View for displaying the favicon.
@property(nonatomic, strong) UIImageView* faviconView;
// Title is displayed by this label.
@property(nonatomic, strong) UILabel* titleLabel;

// Checks if cell has a specific identifier.
- (BOOL)hasIdentifier:(NSString*)identifier;

@property(nonatomic, readonly) UIDragPreviewParameters* dragPreviewParameters;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_PINNED_TABS_PINNED_CELL_H_
