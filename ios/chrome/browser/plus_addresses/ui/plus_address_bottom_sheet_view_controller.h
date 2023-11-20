// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PLUS_ADDRESSES_UI_PLUS_ADDRESS_BOTTOM_SHEET_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_PLUS_ADDRESSES_UI_PLUS_ADDRESS_BOTTOM_SHEET_VIEW_CONTROLLER_H_

#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"

@protocol PlusAddressBottomSheetHandler;

// Plus Address Bottom Sheet UI, which will eventually include a description of
// the feature, a preview of the plus address that can be filled, a button to
// use the plus address and a button to cancel the process and dismiss the UI.
// For now, however, it is a skeleton implementation.
@interface PlusAddressBottomSheetViewController
    : ConfirmationAlertViewController

@end

#endif  // IOS_CHROME_BROWSER_PLUS_ADDRESSES_UI_PLUS_ADDRESS_BOTTOM_SHEET_VIEW_CONTROLLER_H_
