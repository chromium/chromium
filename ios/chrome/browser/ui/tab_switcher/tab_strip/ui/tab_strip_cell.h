// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_UI_TAB_STRIP_CELL_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_UI_TAB_STRIP_CELL_H_

#import <UIKit/UIKit.h>

// UICollectionViewCell which represents an item in TabStripViewController.
@interface TabStripCell : UICollectionViewCell

// The title of the cell.
@property(nonatomic, copy) NSString* title;

// Preview parameters of the cell when dragged.
@property(nonatomic, readonly) UIDragPreviewParameters* dragPreviewParameters;

// Sets the color of this cell's group stroke.
// Subclasses should override this method. Default implementation is no-op.
- (void)setGroupStrokeColor:(UIColor*)groupStrokeColor
    NS_SWIFT_NAME(setGroupStrokeColor(_:));

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_UI_TAB_STRIP_CELL_H_
