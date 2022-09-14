// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_SAVE_CARD_MODAL_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_SAVE_CARD_MODAL_CONSUMER_H_

#import <Foundation/Foundation.h>

namespace {
// Pref keys passed through setupModalViewControllerWithPrefs:.
NSString* kCardholderNamePrefKey = @"cardholderName";
NSString* kCardIssuerIconNamePrefKey = @"cardIssuerIconName";
NSString* kCardNumberPrefKey = @"cardNumber";
NSString* kExpirationMonthPrefKey = @"expirationMonth";
NSString* kExpirationYearPrefKey = @"expirationYear";
NSString* kLegalMessagesPrefKey = @"legalMessages";
NSString* kCurrentCardSavedPrefKey = @"currentCardSaved";
NSString* kSupportsEditingPrefKey = @"supportsEditing";
NSString* kDisplayedTargetAccountEmailPrefKey = @"displayedTargetAccountEmail";
NSString* kDisplayedTargetAccountAvatarPrefKey =
    @"displayedTargetAccountAvatar";
}  // namespace

// Consumer for model to push configurations to the SaveCard UI.
@protocol InfobarSaveCardModalConsumer <NSObject>

// Informs the consumer of the current state of important prefs.
- (void)setupModalViewControllerWithPrefs:(NSDictionary*)prefs;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_SAVE_CARD_MODAL_CONSUMER_H_
