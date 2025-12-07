// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/payments/card_unmask_challenge_option.h"
#import "ios/web_view/internal/autofill/cwv_card_unmask_challenge_option_internal.h"

NS_ASSUME_NONNULL_BEGIN

namespace {

CWVCardUnmaskChallengeOptionType CWVTypeFromAutofillType(
    autofill::CardUnmaskChallengeOptionType type) {
  switch (type) {
    case autofill::CardUnmaskChallengeOptionType::kSmsOtp:
      return CWVCardUnmaskChallengeOptionTypeSmsOtp;
    case autofill::CardUnmaskChallengeOptionType::kCvc:
      return CWVCardUnmaskChallengeOptionTypeCvc;
    case autofill::CardUnmaskChallengeOptionType::kEmailOtp:
      return CWVCardUnmaskChallengeOptionTypeEmailOtp;
    case autofill::CardUnmaskChallengeOptionType::kThreeDomainSecure:
      // Intentionally not exposed in public header, map to Unknown.
      return CWVCardUnmaskChallengeOptionTypeUnknown;
    case autofill::CardUnmaskChallengeOptionType::kUnknownType:
      return CWVCardUnmaskChallengeOptionTypeUnknown;
  }
}

}  // namespace

@implementation CWVCardUnmaskChallengeOption

@synthesize identifier = _identifier;
@synthesize type = _type;
@synthesize challengeLabel = _challengeLabel;
@synthesize challengeInputLength = _challengeInputLength;

- (instancetype)initWithChallengeOption:
    (const autofill::CardUnmaskChallengeOption&)option {
  self = [super init];
  if (self) {
    _identifier = base::SysUTF8ToNSString(option.id.value());
    _type = CWVTypeFromAutofillType(option.type);
    _challengeLabel = base::SysUTF16ToNSString(option.challenge_info);
    _challengeInputLength =
        static_cast<NSInteger>(option.challenge_input_length);
  }
  return self;
}

@end

NS_ASSUME_NONNULL_END
