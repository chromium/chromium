// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_SAVE_CARD_MODAL_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_SAVE_CARD_MODAL_DELEGATE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/infobars/modals/infobar_modal_delegate.h"

class GURL;

// Delegate to handle Save Card Infobar Modal actions.
@protocol InfobarSaveCardModalDelegate <InfobarModalDelegate>

// Saves the current card with using |cardholderName| as cardholder name,
// |month| as expiration month and |year| as expiration year.
- (void)saveCardWithCardholderName:(NSString*)cardholderName
                   expirationMonth:(NSString*)month
                    expirationYear:(NSString*)year;

// Opens |linkURL| in a new tab and dismisses the Modal.
- (void)dismissModalAndOpenURL:(const GURL&)linkURL;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_SAVE_CARD_MODAL_DELEGATE_H_
