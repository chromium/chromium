// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/app_store_rating/constants.h"
#import "components/version_info/channel.h"
#import "ios/chrome/common/channel_info.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kAppStoreRatingTotalDaysOnChromeKey =
    @"AppStoreRatingTotalDaysOnChrome";
NSString* const kAppStoreRatingActiveDaysInPastWeekKey =
    @"AppStoreRatingActiveDaysInPastWeek";
NSString* const kAppStoreRatingLastShownPromoDayKey =
    @"AppStoreRatingLastShownPromoDay";
const unsigned int kAppStoreRatingTotalDaysOnChromeRequirement =
    (GetChannel() == version_info::Channel::DEV ||
     GetChannel() == version_info::Channel::CANARY)
        ? 1
        : 15;
const unsigned int kAppStoreRatingDaysOnChromeInPastWeekRequirement =
    (GetChannel() == version_info::Channel::DEV ||
     GetChannel() == version_info::Channel::CANARY)
        ? 1
        : 3;
