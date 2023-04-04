// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_WHATS_NEW_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_WHATS_NEW_CONSTANTS_H_

@class NSString;

// Key to store whether the What's New promo has been register.
extern NSString* const kWhatsNewPromoRegistrationKey;

// Key to store the date of FRE.
extern NSString* const kWhatsNewDaysAfterFre;

// Key to store the number of launches after FRE.
extern NSString* const kWhatsNewLaunchesAfterFre;

// Key to store whether a user interacted with What's New from the overflow
// menu.
extern NSString* const kWhatsNewUsageEntryKey;

#endif  // IOS_CHROME_BROWSER_UI_WHATS_NEW_CONSTANTS_H_
