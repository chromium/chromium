// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_CELLS_AUTOFILL_EDIT_PROFILE_BUTTON_FOOTER_ITEM_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_CELLS_AUTOFILL_EDIT_PROFILE_BUTTON_FOOTER_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_header_footer_item.h"

@protocol AutofillEditProfileButtonFooterDelegate <NSObject>

// Notifies the delegate that the button was pressed.
- (void)didTapButton;

@end

@interface AutofillEditProfileButtonFooterItem : TableViewHeaderFooterItem

// Text for cell button.
@property(nonatomic, strong) NSString* buttonText;

@end

@interface AutofillEditProfileButtonFooterCell : UITableViewHeaderFooterView

// ReuseID for this class.
@property(class, readonly) NSString* reuseID;

// Delegate to notify when the button is tapped.
@property(nonatomic, weak) id<AutofillEditProfileButtonFooterDelegate> delegate;

// Action button. Note: Set action method in the TableView datasource method.
@property(nonatomic, strong) UIButton* button;

// Updates the button color based on it's status.
- (void)updateButtonColorBasedOnStatus;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_CELLS_AUTOFILL_EDIT_PROFILE_BUTTON_FOOTER_ITEM_H_
