// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/modals/autofill_address_profile/infobar_save_address_profile_modal_consumer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kAddressPrefKey = @"AddressPrefKey";
NSString* const kPhonePrefKey = @"PhonePrefKey";
NSString* const kEmailPrefKey = @"EmailPrefKey";
NSString* const kCurrentAddressProfileSavedPrefKey =
    @"CurrentAddressProfileSavedKey";
NSString* const kIsUpdateModalPrefKey = @"IsUpdateModalPrefKey";
NSString* const kProfileDataDiffKey = @"ProfileDataDiffKey";
NSString* const kUpdateModalDescriptionKey = @"UpdateModalDescriptionKey";
NSString* const kUserEmailKey = @"UserEmailKey";
NSString* const kIsMigrationToAccountKey = @"IsMigrationToAccountKey";
NSString* const kIsProfileAnAccountProfileKey = @"IsProfileAnAccountProfileKey";
NSString* const kProfileDescriptionForMigrationPromptKey =
    @"ProfileDescriptionForMigrationPromptKey";
