// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_UI_BUNDLED_MODALS_INFOBAR_SAVE_CARD_MODAL_DELEGATE_H_
#define IOS_CHROME_BROWSER_INFOBARS_UI_BUNDLED_MODALS_INFOBAR_SAVE_CARD_MODAL_DELEGATE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/infobars/ui_bundled/modals/infobar_modal_delegate.h"

class GURL;

// Delegate to handle Save Card Infobar Modal actions.
@protocol InfobarSaveCardModalDelegate <InfobarModalDelegate>

// Saves the current card with using `cardholderName` as cardholder name,
// `month` as expiration month, `year` as expiration year, `cardCvc` as security
// code. The `cardCvc` is optional and should be an empty string if not provided
// by the user.
- (void)saveCardWithCardholderName:(NSString*)cardholderName
                   expirationMonth:(NSString*)month
                    expirationYear:(NSString*)year
                           cardCvc:(NSString*)cardCvc;

// Opens `linkURL` in a new tab and dismisses the Modal.
- (void)dismissModalAndOpenURL:(const GURL&)linkURL;

@end

#endif  // IOS_CHROME_BROWSER_INFOBARS_UI_BUNDLED_MODALS_INFOBAR_SAVE_CARD_MODAL_DELEGATE_H_
