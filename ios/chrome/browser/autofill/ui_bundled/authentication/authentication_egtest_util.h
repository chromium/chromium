// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTHENTICATION_AUTHENTICATION_EGTEST_UTIL_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTHENTICATION_AUTHENTICATION_EGTEST_UTIL_H_

#import <Foundation/Foundation.h>

// The test page url.
extern const char kCreditCardUrl[];
// A string on the credit_card.html page used to know when the page has loaded.
extern const char kAutofillTestString[];
// The name of the card name form input.
extern const char kFormCardName[];

// The url to intercept in order to inject card unmask responses. These tests
// do not make requests to the real server.
extern NSString* const kUnmaskCardRequestUrl;
// The fake response from the payment server when OTP is available.
extern NSString* const kUnmaskCardResponseOtpSuccess;
// The fake response from the payment server when OTP, email and CVC card unmask
// options are available.
extern NSString* const kUnmaskCardResponseSuccessOtpAndEmailAndCvc;

// The url to intercept in order to inject select challenge option responses.
// These tests do not make requests to the real server.
extern NSString* const kSelectChallengeOptionRequestUrl;
// The fake response from the payment server when an OTP code is successfully
// sent out.
extern NSString* const kSelectChallengeOptionResponseSuccess;

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTHENTICATION_AUTHENTICATION_EGTEST_UTIL_H_
