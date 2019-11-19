// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CREDIT_CARD_SCANNER_CREDIT_CARD_SCANNER_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CREDIT_CARD_SCANNER_CREDIT_CARD_SCANNER_MEDIATOR_DELEGATE_H_

@class CreditCardScannerMediator;

// This delegate is notified when a credit card is scanned.
@protocol CreditCardScannerMediatorDelegate

// Notifies that the scanner has finished scanning a credit card.
- (void)creditCardScannerMediatorDidFinishScan:
    (CreditCardScannerMediator*)mediator API_AVAILABLE(ios(13.0));

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CREDIT_CARD_SCANNER_CREDIT_CARD_SCANNER_MEDIATOR_DELEGATE_H_
