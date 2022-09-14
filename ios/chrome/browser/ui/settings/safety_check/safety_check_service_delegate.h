// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_SAFETY_CHECK_SAFETY_CHECK_SERVICE_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_SAFETY_CHECK_SAFETY_CHECK_SERVICE_DELEGATE_H_

#import <UIKit/UIKit.h>

@class TableViewItem;

// Protocol to handle user actions from the safety check view.
@protocol SafetyCheckServiceDelegate <NSObject>

// Called when item is tapped.
- (void)didSelectItem:(TableViewItem*)item;

// Determines if selection animation should be shown for `item`.
- (BOOL)isItemClickable:(TableViewItem*)item;

// Checks if `item` should have an error popover.
- (BOOL)isItemWithErrorInfo:(TableViewItem*)item;

// Notifies the mediator that an info button was tapped for `itemType`.
- (void)infoButtonWasTapped:(UIButton*)buttonView
              usingItemType:(NSInteger)itemType;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_SAFETY_CHECK_SAFETY_CHECK_SERVICE_DELEGATE_H_
