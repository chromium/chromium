// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_sharing/family_promo_mediator.h"

#import "ios/chrome/browser/ui/settings/password/password_sharing/family_promo_consumer.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/password_sharing_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

@implementation FamilyPromoMediator {
  // Type of the family promo to be displayed by the view controller.
  FamilyPromoType _familyPromoType;
}

- (instancetype)initWithFamilyPromoType:(FamilyPromoType)familyPromoType {
  self = [super init];
  if (self) {
    _familyPromoType = familyPromoType;
  }
  return self;
}

- (void)setConsumer:(id<FamilyPromoConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }

  _consumer = consumer;
  [_consumer setTitle:[self title] subtitle:[self subtitle]];
}

#pragma mark - Private

// Returns subtitle based on the `_familyPromoType`.
- (NSString*)subtitle {
  switch (_familyPromoType) {
    case FamilyPromoType::kUserNotInFamilyGroup:
      return l10n_util::GetNSString(
          IDS_IOS_PASSWORD_SHARING_FAMILY_PROMO_SUBTITLE);
    case FamilyPromoType::kUserWithNoOtherFamilyMembers:
      return l10n_util::GetNSString(
          IDS_IOS_PASSWORD_SHARING_FAMILY_PROMO_INVITE_MEMBERS_SUBTITLE);
  }
}

// Returns title based on the `_familyPromoType`.
- (NSString*)title {
  switch (_familyPromoType) {
    case FamilyPromoType::kUserNotInFamilyGroup:
      return l10n_util::GetNSString(
          IDS_IOS_PASSWORD_SHARING_FAMILY_PROMO_TITLE);
    case FamilyPromoType::kUserWithNoOtherFamilyMembers:
      return l10n_util::GetNSString(
          IDS_IOS_PASSWORD_SHARING_FAMILY_PROMO_INVITE_MEMBERS_TITLE);
  }
}

@end
