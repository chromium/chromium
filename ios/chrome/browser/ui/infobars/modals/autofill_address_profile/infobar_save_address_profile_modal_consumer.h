// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_AUTOFILL_ADDRESS_PROFILE_INFOBAR_SAVE_ADDRESS_PROFILE_MODAL_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_AUTOFILL_ADDRESS_PROFILE_INFOBAR_SAVE_ADDRESS_PROFILE_MODAL_CONSUMER_H_

#import <Foundation/Foundation.h>

namespace {
// Pref keys passed through setupModalViewControllerWithPrefs:.
NSString* kAddressPrefKey = @"AddressPrefKey";
NSString* kPhonePrefKey = @"PhonePrefKey";
NSString* kEmailPrefKey = @"EmailPrefKey";
NSString* kCurrentAddressProfileSavedPrefKey = @"CurrentAddressProfileSavedKey";
NSString* kIsUpdateModalPrefKey = @"IsUpdateModalPrefKey";
NSString* kProfileDataDiffKey = @"ProfileDataDiffKey";
NSString* kUpdateModalDescriptionKey = @"UpdateModalDescriptionKey";
NSString* kSyncingUserEmailKey = @"SyncingUserEmailKey";
NSString* kIsMigrationToAccountKey = @"IsMigrationToAccountKey";
NSString* kIsProfileAnAccountProfileKey = @"IsProfileAnAccountProfileKey";
NSString* kProfileDescriptionForMigrationPromptKey =
    @"ProfileDescriptionForMigrationPromptKey";
}  // namespace

// Consumer for model to push configurations to the SaveAddressProfile UI.
@protocol InfobarSaveAddressProfileModalConsumer <NSObject>

// Informs the consumer of the current state of important prefs.
- (void)setupModalViewControllerWithPrefs:(NSDictionary*)prefs;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_AUTOFILL_ADDRESS_PROFILE_INFOBAR_SAVE_ADDRESS_PROFILE_MODAL_CONSUMER_H_
