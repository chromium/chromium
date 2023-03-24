// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_UTIL_H_
#define IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_UTIL_H_

#import <Foundation/Foundation.h>

class PromosManager;

// Key to store whether the What's New promo has been register.
extern NSString* const kWhatsNewPromoRegistrationKey;

// Key to store the date of FRE.
extern NSString* const kWhatsNewDaysAfterFre;

// Key to store the number of launches after FRE.
extern NSString* const kWhatsNewLaunchesAfterFre;

// Key to store whether a user interacted with What's New from the overflow
// menu.
extern NSString* const kWhatsNewUsageEntryKey;

// Returns whether What's New was used in the overflow menu. This is used to
// decide on the location of the What's New entry point in the overflow menu.
bool WasWhatsNewUsed();

// Set that What's New was used in the overflow menu.
void SetWhatsNewUsed(PromosManager* promosManager);

// Returns whether What's New is enabled.
bool IsWhatsNewEnabled();

// Set that What's New has been registered in the promo manager.
void setWhatsNewPromoRegistration();

// Returns whether What's New promo should be registered in the promo manager.
// This is used to avoid registering the What's New promo in the promo manager
// more than once.
bool ShouldRegisterWhatsNewPromo();

#endif  // IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_UTIL_H_
