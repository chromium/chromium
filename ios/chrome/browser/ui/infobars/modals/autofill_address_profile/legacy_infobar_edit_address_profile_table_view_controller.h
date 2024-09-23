// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_AUTOFILL_ADDRESS_PROFILE_LEGACY_INFOBAR_EDIT_ADDRESS_PROFILE_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_AUTOFILL_ADDRESS_PROFILE_LEGACY_INFOBAR_EDIT_ADDRESS_PROFILE_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/autofill/ui_bundled/autofill_profile_edit_handler.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller.h"
#import "ios/chrome/browser/ui/infobars/modals/autofill_address_profile/infobar_edit_address_profile_modal_consumer.h"

@protocol InfobarModalDelegate;

// The TableView for an Autofill save/update address edit menu.
@interface LegacyInfobarEditAddressProfileTableViewController
    : LegacyChromeTableViewController <InfobarEditAddressProfileModalConsumer,
                                       UITextFieldDelegate>

- (instancetype)initWithModalDelegate:(id<InfobarModalDelegate>)delegate;

@property(nonatomic, weak) id<AutofillProfileEditHandler> handler;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_AUTOFILL_ADDRESS_PROFILE_LEGACY_INFOBAR_EDIT_ADDRESS_PROFILE_TABLE_VIEW_CONTROLLER_H_
