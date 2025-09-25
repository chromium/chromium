// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AIM_PROTOTYPE_UI_AIM_INPUT_ITEM_VIEW_H_
#define IOS_CHROME_BROWSER_AIM_PROTOTYPE_UI_AIM_INPUT_ITEM_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/aim/prototype/ui/aim_input_item.h"

/// Image input item size.
extern const CGSize kImageInputItemSize;
/// Tab/File input item size.
extern const CGSize kTabFileInputItemSize;

// The aim input item view.
@interface AimInputItemView : UIView

/// The close button.
@property(nonatomic, strong, readonly) UIButton* closeButton;

/// Updates the UI based on the item's type.
- (void)configureWithItem:(AIMInputItem*)item;

/// Prepares the view for reuse.
- (void)prepareForReuse;

@end

#endif  // IOS_CHROME_BROWSER_AIM_PROTOTYPE_UI_AIM_INPUT_ITEM_VIEW_H_
