// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_BNPL_PUBLIC_BNPL_ISSUER_DATA_H_
#define IOS_CHROME_BROWSER_AUTOFILL_BNPL_PUBLIC_BNPL_ISSUER_DATA_H_

#import <UIKit/UIKit.h>

#import "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"

// Data source for each individual BNPL issuer.
@interface BnplIssuerData : NSObject

// The issuer name.
@property(readonly, copy, nonatomic) NSString* issuerName;

// The issuer ID.
@property(readonly, nonatomic) autofill::BnplIssuer::IssuerId issuerId;

// The display text shown underneath the issuer name.
@property(readonly, copy, nonatomic) NSString* selectionOptionText;

// The icon associated with the BNPL issuer.
@property(readonly, strong, nonatomic) UIImage* icon;

// Whether the BNPL issuer is linked to the user's Google account.
@property(readonly, nonatomic, getter=isLinked) BOOL linked;

// Initializes from the autofill BNPL issuer, the selection option text, and an
// icon.
- (instancetype)initWithBnplIssuer:(const autofill::BnplIssuer&)bnplIssuer
               selectionOptionText:(NSString*)selectionOptionText
                              icon:(UIImage*)icon;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_BNPL_PUBLIC_BNPL_ISSUER_DATA_H_
