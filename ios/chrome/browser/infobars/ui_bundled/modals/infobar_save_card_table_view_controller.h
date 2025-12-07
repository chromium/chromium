// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_UI_BUNDLED_MODALS_INFOBAR_SAVE_CARD_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_INFOBARS_UI_BUNDLED_MODALS_INFOBAR_SAVE_CARD_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/infobars/ui_bundled/modals/infobar_save_card_modal_consumer.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller.h"

class GURL;

@protocol InfobarSaveCardTableViewControllerContainerDelegate

// Updates the save button state.
- (void)updateSaveButtonEnabled:(BOOL)enabled;

// Updates the save button to show progress.
- (void)showProgressWithUploadCompleted:(BOOL)uploadCompleted;

// Asks the container to dismiss the modal and open the URL.
- (void)dismissModalAndOpenURL:(const GURL&)URL;

@end

// InfobarSaveCardTableViewController represents the content for the Save Card
// InfobarModal.
@interface InfobarSaveCardTableViewController
    : LegacyChromeTableViewController <InfobarSaveCardModalConsumer>

// Delegate containing the view controller.
@property(nonatomic, weak)
    id<InfobarSaveCardTableViewControllerContainerDelegate>
        containerDelegate;

// Returns the current values of the card fields.
@property(nonatomic, readonly) NSString* currentCardholderName;
@property(nonatomic, readonly) NSString* currentExpirationMonth;
@property(nonatomic, readonly) NSString* currentExpirationYear;
@property(nonatomic, readonly) NSString* currentCardCVC;

@end

#endif  // IOS_CHROME_BROWSER_INFOBARS_UI_BUNDLED_MODALS_INFOBAR_SAVE_CARD_TABLE_VIEW_CONTROLLER_H_
