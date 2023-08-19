// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_WHATS_NEW_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_WHATS_NEW_CONSTANTS_H_

@class NSString;

// Key to store whether the What's New promo has been register.
extern NSString* const kWhatsNewPromoRegistrationKey;

// Key to store whether the What's New M116 promo has been register.
extern NSString* const kWhatsNewM116PromoRegistrationKey;

// Key to store the date of FRE.
extern NSString* const kWhatsNewDaysAfterFre;

// Key to store the number of launches after FRE.
extern NSString* const kWhatsNewLaunchesAfterFre;

// Key to store the date of FRE What's New M116.
extern NSString* const kWhatsNewM116DaysAfterFre;

// Key to store the number of launches after FRE for What's New M116.
extern NSString* const kWhatsNewM116LaunchesAfterFre;

// Key to store whether a user interacted with What's New from the overflow
// menu.
extern NSString* const kWhatsNewUsageEntryKey;

// Key to store whether a user interacted with What's New M116 from the overflow
// menu.
extern NSString* const kWhatsNewM116UsageEntryKey;

#endif  // IOS_CHROME_BROWSER_UI_WHATS_NEW_CONSTANTS_H_
