// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/authentication/authentication_egtest_util.h"

const char kCreditCardUrl[] = "/credit_card.html";
const char kAutofillTestString[] = "Autofill Test";
const char kFormCardName[] = "CCName";

NSString* const kUnmaskCardRequestUrl =
    @"https://payments.google.com/payments/apis-secure/creditcardservice/"
    @"getrealpan?s7e_suffix=chromewallet";
NSString* const kUnmaskCardResponseOtpSuccess =
    @"{\"context_token\":\"fake_context_token\",\"idv_challenge_options\":["
    @"{\"sms_otp_challenge_option\":{\"challenge_id\":"
    @"\"JGQ1YTkxM2ZjLWY4YTAtMTFlZS1hMmFhLWZmYjYwNWVjODcwMwo=\",\"masked_phone_"
    @"number\":\"*******1234\",\"otp_length\":6}}]}";
NSString* const kUnmaskCardResponseSuccessOtpAndEmailAndCvc =
    @"{\"context_token\":\"__fake_context_token__\",\"idv_challenge_options\":["
    @"{\"sms_otp_challenge_option\":{\"challenge_id\":"
    @"\"JGQ1YTkxM2ZjLWY4YTAtMTFlZS1hMmFhLWZmYjYwNWVjODcwMwo=\",\"masked_phone_"
    @"number\":\"*******1234\",\"otp_length\":6}},{\"email_otp_challenge_"
    @"option\":{\"challenge_id\":"
    @"\"JDNhNTdlMzVhLWY4YTEtMTFlZS1hOTUwLWZiNzY3ZWM4ZWY3ZAo=\",\"masked_email_"
    @"address\":\"a***b@gmail.com\",\"otp_length\":6}},{\"cvc_challenge_"
    @"option\":{\"challenge_id\":\"hardcoded_3CSC_challenge_id\",\"cvc_"
    @"length\":3,\"cvc_position\":\"CVC_POSITION_BACK\"}}]}";

NSString* const kSelectChallengeOptionRequestUrl =
    @"https://payments.google.com/payments/apis/chromepaymentsservice/"
    @"selectchallengeoption";
NSString* const kSelectChallengeOptionResponseSuccess =
    @"{\"context_token\":\"fake_context_token\"}";
