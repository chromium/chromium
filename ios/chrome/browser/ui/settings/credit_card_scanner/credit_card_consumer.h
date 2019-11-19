// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CREDIT_CARD_SCANNER_CREDIT_CARD_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CREDIT_CARD_SCANNER_CREDIT_CARD_CONSUMER_H_

// Objects conforming to this protocol need to react when new credit
// card data is available.
@protocol CreditCardConsumer

// Notifies the consumer of new data being available.
- (void)setCreditCardNumber:(NSString*)cardNumber
            expirationMonth:(NSString*)expirationMonth
             expirationYear:(NSString*)expirationYear;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CREDIT_CARD_SCANNER_CREDIT_CARD_CONSUMER_H_
