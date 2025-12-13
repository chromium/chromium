// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_UI_BUNDLED_MODALS_AUTOFILL_ADDRESS_PROFILE_INFOBAR_SAVE_ADDRESS_PROFILE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_INFOBARS_UI_BUNDLED_MODALS_AUTOFILL_ADDRESS_PROFILE_INFOBAR_SAVE_ADDRESS_PROFILE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/infobars/ui_bundled/modals/autofill_address_profile/infobar_save_address_profile_modal_consumer.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller.h"

@protocol InfobarSaveAddressProfileModalDelegate;

// InfobarSaveAddressProfileViewController represents the content for the
// Save Address Profile InfobarModal.
@interface InfobarSaveAddressProfileViewController
    : UIViewController <InfobarSaveAddressProfileModalConsumer>

- (instancetype)initWithModalDelegate:
    (id<InfobarSaveAddressProfileModalDelegate>)modalDelegate
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_INFOBARS_UI_BUNDLED_MODALS_AUTOFILL_ADDRESS_PROFILE_INFOBAR_SAVE_ADDRESS_PROFILE_VIEW_CONTROLLER_H_
