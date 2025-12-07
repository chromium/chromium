// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_CARD_UNMASK_CHALLENGE_OPTION_INTERNAL_H_
#define IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_CARD_UNMASK_CHALLENGE_OPTION_INTERNAL_H_

#import "ios/web_view/public/cwv_card_unmask_challenge_option.h"

// Forward declaration of the C++ type.
namespace autofill {
struct CardUnmaskChallengeOption;
}

NS_ASSUME_NONNULL_BEGIN

@interface CWVCardUnmaskChallengeOption (Internal)

// Initializes the Objective-C wrapper from the C++ struct.
- (instancetype)initWithChallengeOption:
    (const autofill::CardUnmaskChallengeOption&)option;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_CARD_UNMASK_CHALLENGE_OPTION_INTERNAL_H_
