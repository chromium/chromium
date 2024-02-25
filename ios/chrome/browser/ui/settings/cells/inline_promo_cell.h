// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_INLINE_PROMO_CELL_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_INLINE_PROMO_CELL_H_

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_cell.h"

// TableViewCell with:
//   - an image overlaid by a new feature badge
//   - a descriptive label
//   - a button to get more info
//   - a close button at the top right
// Narrow layout: the image, the label and the more info button are displayed in
// a single centered column.
// Wide layout: the layout has two left aligned columns. One for the image and
// another one for the label and the more info button.
//
// Used to display a promo within a table view.
@interface InlinePromoCell : TableViewCell

// Button to dismiss the promo.
@property(nonatomic, readonly) UIButton* closeButton;

// Image view of the cell.
@property(nonatomic, readonly) UIImageView* promoImageView;

// Label containing the description of the promo.
@property(nonatomic, readonly) UILabel* promoTextLabel;

// Button to get more info on the promo.
@property(nonatomic, readonly) UIButton* moreInfoButton;

// Whether or not the cell can be interacted with. When the cell is disabled,
// the text is dimmed and colors, as well as the image, are greyed out. `YES` by
// default.
@property(nonatomic, assign, getter=isEnabled) BOOL enabled;

// Whether or not the cell should be configured with its wide layout.
@property(nonatomic, assign) BOOL shouldHaveWideLayout;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_INLINE_PROMO_CELL_H_
