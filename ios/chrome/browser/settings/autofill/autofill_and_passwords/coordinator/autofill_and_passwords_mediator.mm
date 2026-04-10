// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/autofill_and_passwords_mediator.h"

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/autofill_and_passwords_consumer.h"

@implementation AutofillAndPasswordsMediator

- (void)setConsumer:(id<AutofillAndPasswordsConsumer>)consumer {
  _consumer = consumer;

  // TODO(crbug.com/491409453): Fetch actual boolean values from PrefService.
  [_consumer setPasswordsEnabled:YES];
  [_consumer setAutofillCreditCardEnabled:YES];
  [_consumer setAutofillProfileEnabled:YES];
}

- (void)disconnect {
}

@end
