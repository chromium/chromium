// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/bnpl/public/bnpl_issuer_data.h"

@implementation BnplIssuerData

- (instancetype)initWithBnplIssuer:(const autofill::BnplIssuer&)bnplIssuer
                              icon:(UIImage*)icon {
  if ((self = [super init])) {
    // TODO(crbug.com/469521271): Implement full property assignments from
    // bnplIssuer.
    _issuerId = bnplIssuer.issuer_id();
    _icon = icon;
  }
  return self;
}

@end
