// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_MODEL_LOCATION_BAR_BADGE_UTIL_H_
#define IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_MODEL_LOCATION_BAR_BADGE_UTIL_H_

#import <optional>

#import "ios/chrome/browser/infobars/model/infobar_type.h"
#import "ios/chrome/browser/location_bar/badge/model/badge_type.h"

// Maps `infobar_type` to `LocationBarBadgeType` if supported, or returns
// `std::nullopt` if `infobar_type` does not display a location bar badge.
// `InfobarType::kInfobarTypePermissions` returns
// `LocationBarBadgeType::kPermissionsCamera` as a default value; the actual
// badge (camera or microphone) depends on `GetStatesForAllPermissions()` of
// the active `WebState` and must be overridden by the caller when used.
// TODO(crbug.com/569998074): Remove `BadgeTypeForInfobarType()` in
// `ios/chrome/browser/badges/ui_bundled/badge_type_util.h` once the Location
// Bar Badge migration completes.
std::optional<LocationBarBadgeType> LocationBarBadgeTypeForInfobarType(
    InfobarType infobar_type);

// Returns whether `badge_type` is a contextual panel entrypoint badge.
bool IsContextualPanelEntryPointBadgeType(LocationBarBadgeType badge_type);

// Returns whether `badge_type` originates from the badge button factory
// (infobars, incognito, and overflow badges).
bool IsBadgeFromFactory(LocationBarBadgeType badge_type);

#endif  // IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_MODEL_LOCATION_BAR_BADGE_UTIL_H_
