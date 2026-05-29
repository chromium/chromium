// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/bnpl/public/bnpl_issuer_data.h"

#import "base/strings/sys_string_conversions.h"

@implementation BnplIssuerData

- (instancetype)initWithBnplIssuer:(const autofill::BnplIssuer&)bnplIssuer
               selectionOptionText:(NSString*)selectionOptionText
                              icon:(UIImage*)icon {
  if ((self = [super init])) {
    _issuerId = bnplIssuer.issuer_id();
    _issuerName = base::SysUTF16ToNSString(bnplIssuer.GetDisplayName());
    _selectionOptionText = [selectionOptionText copy];
    _icon = icon;
    _linked = bnplIssuer.payment_instrument().has_value();
  }
  return self;
}

@end
