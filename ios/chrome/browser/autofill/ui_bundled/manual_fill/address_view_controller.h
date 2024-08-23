// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_ADDRESS_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_ADDRESS_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/address_consumer.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/fallback_view_controller.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_plus_address_consumer.h"

// This class presents a list of usernames and addresess in a table view.
@interface AddressViewController
    : FallbackViewController <ManualFillAddressConsumer,
                              ManualFillPlusAddressConsumer>

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_ADDRESS_VIEW_CONTROLLER_H_
