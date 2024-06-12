// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BADGES_UI_BUNDLED_BADGE_TYPE_UTIL_H_
#define IOS_CHROME_BROWSER_BADGES_UI_BUNDLED_BADGE_TYPE_UTIL_H_

#import "ios/chrome/browser/infobars/model/infobar_type.h"
#import "ios/chrome/browser/badges/ui_bundled/badge_type.h"

// Returns the corresponding BadgeType for `infobar_type`, or kBadgeTypeNone
// if that InfobarType does not support badges.
BadgeType BadgeTypeForInfobarType(InfobarType infobar_type);

// Returns the corresponding InfobarType for `badge_type`. `badge_type` must
// not be kBadgeTypeNone.
InfobarType InfobarTypeForBadgeType(BadgeType badge_type);

#endif  // IOS_CHROME_BROWSER_BADGES_UI_BUNDLED_BADGE_TYPE_UTIL_H_
