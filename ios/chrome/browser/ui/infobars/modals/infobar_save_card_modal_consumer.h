// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_SAVE_CARD_MODAL_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_SAVE_CARD_MODAL_CONSUMER_H_

#import <Foundation/Foundation.h>

// Pref keys passed through setupModalViewControllerWithPrefs:.
extern NSString* const kCardholderNamePrefKey;
extern NSString* const kCardIssuerIconNamePrefKey;
extern NSString* const kCardNumberPrefKey;
extern NSString* const kExpirationMonthPrefKey;
extern NSString* const kExpirationYearPrefKey;
extern NSString* const kLegalMessagesPrefKey;
extern NSString* const kCurrentCardSaveAcceptedPrefKey;
extern NSString* const kSupportsEditingPrefKey;
extern NSString* const kDisplayedTargetAccountEmailPrefKey;
extern NSString* const kDisplayedTargetAccountAvatarPrefKey;

// Consumer for model to push configurations to the SaveCard UI.
@protocol InfobarSaveCardModalConsumer <NSObject>

// Informs the consumer of the current state of important prefs.
- (void)setupModalViewControllerWithPrefs:(NSDictionary*)prefs;

// Updates modal to show progress of card upload. With `uploadCompleted` as
// `NO`, informs the consumer to show loading state after save card button is
// pressed. With `uploadCompleted` as `YES`, informs the consumer to show card
// upload success.
- (void)showProgressWithUploadCompleted:(BOOL)uploadCompleted;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_SAVE_CARD_MODAL_CONSUMER_H_
