// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_AUTOFILL_ADDRESS_PROFILE_INFOBAR_SAVE_ADDRESS_PROFILE_MODAL_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_AUTOFILL_ADDRESS_PROFILE_INFOBAR_SAVE_ADDRESS_PROFILE_MODAL_CONSUMER_H_

#import <Foundation/Foundation.h>

// Pref keys passed through setupModalViewControllerWithPrefs:.
extern NSString* const kAddressPrefKey;
extern NSString* const kPhonePrefKey;
extern NSString* const kEmailPrefKey;
extern NSString* const kCurrentAddressProfileSavedPrefKey;
extern NSString* const kIsUpdateModalPrefKey;
extern NSString* const kProfileDataDiffKey;
extern NSString* const kUpdateModalDescriptionKey;
extern NSString* const kUserEmailKey;
extern NSString* const kIsMigrationToAccountKey;
extern NSString* const kIsProfileAnAccountProfileKey;
extern NSString* const kProfileDescriptionForMigrationPromptKey;

// Consumer for model to push configurations to the SaveAddressProfile UI.
@protocol InfobarSaveAddressProfileModalConsumer <NSObject>

// Informs the consumer of the current state of important prefs.
- (void)setupModalViewControllerWithPrefs:(NSDictionary*)prefs;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_AUTOFILL_ADDRESS_PROFILE_INFOBAR_SAVE_ADDRESS_PROFILE_MODAL_CONSUMER_H_
