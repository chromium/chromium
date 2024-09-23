// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_ALL_PLUS_ADDRESS_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_ALL_PLUS_ADDRESS_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/fallback_view_controller.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_plus_address_consumer.h"

@class CrURL;
@class ManualFillAllPlusAddressViewController;

// Delegate of the view controller.
@protocol ManualFillAllPlusAddressViewControllerDelegate

// User tapped the "Done" button.
- (void)selectPlusAddressViewControllerDidTapDoneButton:
    (ManualFillAllPlusAddressViewController*)selectPlusAddressViewController;

@end

// This class presents a list of plus addresses in a table view.
@interface ManualFillAllPlusAddressViewController
    : FallbackViewController <ManualFillPlusAddressConsumer>

// Delegate for the view controller.
@property(nonatomic, weak) id<ManualFillAllPlusAddressViewControllerDelegate>
    delegate;

- (instancetype)initWithSearchController:(UISearchController*)searchController
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_ALL_PLUS_ADDRESS_VIEW_CONTROLLER_H_
