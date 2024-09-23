// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTHENTICATION_CARD_UNMASK_CHALLENGE_OPTION_IOS_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTHENTICATION_CARD_UNMASK_CHALLENGE_OPTION_IOS_H_

#import "components/autofill/core/browser/payments/card_unmask_challenge_option.h"

#import <Foundation/Foundation.h>

// Represents a challenge option on the authentication selection prompt.
@interface CardUnmaskChallengeOptionIOS : NSObject

// The identifier of the option.
@property(nonatomic, readonly)
    autofill::CardUnmaskChallengeOption::ChallengeOptionId id;

// The option's title.
@property(nonatomic, readonly) NSString* modeLabel;

// Optional additional information regarding this challenge option, for example,
// the truncated phone number on an SMS challenge option.
@property(nonatomic, readonly) NSString* challengeInfo;

// Creates the Challenge option given all its properties.
- (instancetype)initWithId:
                    (autofill::CardUnmaskChallengeOption::ChallengeOptionId)id
                 modeLabel:(NSString*)modeLabel
             challengeInfo:(NSString*)challengInfo;

// Convert this option from the autofill C++ equivalent,
// CardUnmaskChallengeOption.
+ (instancetype)convertFrom:
                    (const autofill::CardUnmaskChallengeOption&)autofillOption
                  modeLabel:(const std::u16string&)modeLabel;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTHENTICATION_CARD_UNMASK_CHALLENGE_OPTION_IOS_H_
