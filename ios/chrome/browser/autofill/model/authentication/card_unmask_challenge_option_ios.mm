// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/authentication/card_unmask_challenge_option_ios.h"

#import "base/hash/hash.h"
#import "base/strings/sys_string_conversions.h"

@implementation CardUnmaskChallengeOptionIOS

- (instancetype)initWithId:
                    (autofill::CardUnmaskChallengeOption::ChallengeOptionId)id
                 modeLabel:(NSString*)modeLabel
             challengeInfo:(NSString*)challengeInfo {
  self = [super init];
  if (self) {
    _id = id;
    _modeLabel = modeLabel;
    _challengeInfo = challengeInfo;
  }
  return self;
}

// Convert this option from the autofill C++ equivalent,
// CardUnmaskChallengeOption.
+ (instancetype)convertFrom:
                    (const autofill::CardUnmaskChallengeOption&)autofillOption
                  modeLabel:(const std::u16string&)modeLabel {
  return [[CardUnmaskChallengeOptionIOS alloc]
         initWithId:autofillOption.id
          modeLabel:base::SysUTF16ToNSString(modeLabel)
      challengeInfo:base::SysUTF16ToNSString(autofillOption.challenge_info)];
}

- (BOOL)isEqual:(id)other {
  if (self == other) {
    return YES;
  }

  if (![other isMemberOfClass:[CardUnmaskChallengeOptionIOS class]]) {
    return NO;
  }

  return [self isEqualToChallengeOptionIOS:other];
}

- (BOOL)isEqualToChallengeOptionIOS:(CardUnmaskChallengeOptionIOS*)other {
  return self->_id == other->_id &&
         [self->_modeLabel isEqualToString:other->_modeLabel] &&
         [self->_challengeInfo isEqualToString:other->_challengeInfo];
}

- (NSUInteger)hash {
  return base::Hash(self->_id.value()) ^ [self->_modeLabel hash] ^
         [self->_challengeInfo hash];
}

@end
