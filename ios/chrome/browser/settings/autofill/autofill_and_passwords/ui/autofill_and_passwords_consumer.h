// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_UI_AUTOFILL_AND_PASSWORDS_CONSUMER_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_UI_AUTOFILL_AND_PASSWORDS_CONSUMER_H_

#import <Foundation/Foundation.h>

// Consumer protocol for the Autofill and Passwords settings page.
@protocol AutofillAndPasswordsConsumer <NSObject>

// Sets the passwords item with detail text.
- (void)setPasswordsEnabled:(BOOL)enabled;

// Sets the autofill credit card item with detail text.
- (void)setAutofillCreditCardEnabled:(BOOL)enabled;

// Sets the autofill profile item with detail text.
- (void)setAutofillProfileEnabled:(BOOL)enabled;

// Sets the identity docs item with detail text.
- (void)setIdentityDocsEnabled:(BOOL)enabled;

// Sets the travel info item with detail text.
- (void)setTravelInfoEnabled:(BOOL)enabled;

// Sets whether to show the Autofill AI features items.
- (void)setShouldShowAutofillAIFeatures:(BOOL)shouldShow;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_UI_AUTOFILL_AND_PASSWORDS_CONSUMER_H_
