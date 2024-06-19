// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTHENTICATION_CARD_UNMASK_AUTHENTICATION_SELECTION_CONSUMER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTHENTICATION_CARD_UNMASK_AUTHENTICATION_SELECTION_CONSUMER_H_

#import "ios/chrome/browser/autofill/model/authentication/card_unmask_challenge_option_ios.h"

// Consumer for card unmask authentication selection prompts.
@protocol CardUnmaskAuthenticationSelectionConsumer

// Sets the primary message typically the largest text on the prompt.
- (void)setHeaderTitle:(NSString*)headerTitle;

// Sets a longer description complementing the header title. Typically shown in
// smaller text.
- (void)setHeaderText:(NSString*)headerText;

// Sets additional text shown after the authentication selection options.
- (void)setFooterText:(NSString*)footerText;

// Sets the label for accepting the challenge option. For example this may be a
// button labelled "Send" when an OTP challenge is selected, or "Continue" when
// CVC is selected.
- (void)setChallengeAcceptanceLabel:(NSString*)challengeAcceptanceLabel;

// Sets the available card unmask options.
- (void)setCardUnmaskOptions:
    (NSArray<CardUnmaskChallengeOptionIOS*>*)cardUnmaskChallengeOptions;

// Sets the pending state. This happens while waiting on the payment server
// after selecting an option.
- (void)enterPendingState;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTHENTICATION_CARD_UNMASK_AUTHENTICATION_SELECTION_CONSUMER_H_
