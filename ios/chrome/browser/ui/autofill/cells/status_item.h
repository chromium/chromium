// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_CELLS_STATUS_ITEM_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_CELLS_STATUS_ITEM_H_

#import <UIKit/UIKit.h>

#import <MaterialComponents/MaterialCollectionCells.h>

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"

enum class StatusItemState {
  VERIFYING,
  VERIFIED,
  ERROR,
};

@class MDCActivityIndicator;

// The status item displays a status indicator and some text indicating the
// current state of a verification attempt.
@interface StatusItem : CollectionViewItem

// The text indicating the current state.
@property(nonatomic, copy) NSString* text;

// The current state of the verification attempt.
@property(nonatomic, assign) StatusItemState state;

@end

// Cell corresponding to a StatusItem.
@interface StatusCell : MDCCollectionViewCell

// The activity indicator to indicate the VERIFYING state.
@property(nonatomic, strong) MDCActivityIndicator* activityIndicator;

// The image view to indicate the VERIFIED state.
@property(nonatomic, strong) UIImageView* verifiedImageView;

// The image view to indicate the ERROR state.
@property(nonatomic, strong) UIImageView* errorImageView;

// The text label to display the message associated with the current state.
@property(nonatomic, strong) UILabel* textLabel;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_CELLS_STATUS_ITEM_H_
