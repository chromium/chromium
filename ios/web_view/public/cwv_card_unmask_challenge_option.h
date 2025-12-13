// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_CARD_UNMASK_CHALLENGE_OPTION_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_CARD_UNMASK_CHALLENGE_OPTION_H_

#import <Foundation/Foundation.h>

#import "cwv_export.h"

NS_ASSUME_NONNULL_BEGIN

// Represents the type of challenge option for unmasking a card.
// This should be kept in sync with autofill::CardUnmaskChallengeOptionType.
typedef NS_ENUM(NSInteger, CWVCardUnmaskChallengeOptionType) {
  CWVCardUnmaskChallengeOptionTypeUnknown = 0,
  CWVCardUnmaskChallengeOptionTypeSmsOtp,
  CWVCardUnmaskChallengeOptionTypeCvc,
  CWVCardUnmaskChallengeOptionTypeEmailOtp,
  // Intentionally not exposing ThreeDomainSecure
};

// Objective-C wrapper for autofill::CardUnmaskChallengeOption.
// Represents a single method/challenge to unmask a credit card.
// Instances of this class are created internally by the framework.

CWV_EXPORT
@interface CWVCardUnmaskChallengeOption : NSObject

// The unique identifier for this challenge option.
@property(nonatomic, readonly, copy) NSString* identifier;

// The type of challenge.
@property(nonatomic, readonly, assign) CWVCardUnmaskChallengeOptionType type;

// The user-facing description of this challenge option (e.g., "Get a code by
// text message").
@property(nonatomic, readonly, copy) NSString* challengeLabel;

// The expected length of the input for this challenge (e.g., CVC length, OTP
// length).
@property(nonatomic, readonly, assign) NSInteger challengeInputLength;

- (instancetype)init NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_CARD_UNMASK_CHALLENGE_OPTION_H_
