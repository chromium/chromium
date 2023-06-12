// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/modals/infobar_save_card_modal_consumer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kCardholderNamePrefKey = @"cardholderName";
NSString* const kCardIssuerIconNamePrefKey = @"cardIssuerIconName";
NSString* const kCardNumberPrefKey = @"cardNumber";
NSString* const kExpirationMonthPrefKey = @"expirationMonth";
NSString* const kExpirationYearPrefKey = @"expirationYear";
NSString* const kLegalMessagesPrefKey = @"legalMessages";
NSString* const kCurrentCardSavedPrefKey = @"currentCardSaved";
NSString* const kSupportsEditingPrefKey = @"supportsEditing";
NSString* const kDisplayedTargetAccountEmailPrefKey =
    @"displayedTargetAccountEmail";
NSString* const kDisplayedTargetAccountAvatarPrefKey =
    @"displayedTargetAccountAvatar";
