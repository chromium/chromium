// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_MODEL_LOCATION_BAR_BADGE_REQUEST_H_
#define IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_MODEL_LOCATION_BAR_BADGE_REQUEST_H_

#import <optional>
#import <string>

#import "ios/chrome/browser/location_bar/badge/model/badge_type.h"

// Encapsulates a request to display a badge in the location bar.
struct LocationBarBadgeRequest {
  // The type of badge to display.
  LocationBarBadgeType badge_type = LocationBarBadgeType::kNone;
  // Priority tier for arbitration among competing badges.
  LocationBarBadgePriority priority = LocationBarBadgePriority::kDefault;
  // Optional localized accessibility label overriding the default badge label.
  std::optional<std::u16string> accessibility_label;
  // Optional accessibility identifier overriding the default identifier.
  std::optional<std::string> accessibility_identifier;
  // Optional localized text displayed alongside the icon when expanded.
  std::optional<std::u16string> label_text;
  // Whether an unread indicator (e.g. blue dot) is displayed on the badge.
  bool has_unread_indicator = false;
  // Whether the underlying prompt/infobar has been accepted by the user.
  bool accepted = false;
  // Whether the badge is highlighted while its banner or modal is presented.
  bool is_highlighted = false;

  // Equality operator for change detection and deduplication.
  friend bool operator==(const LocationBarBadgeRequest&,
                         const LocationBarBadgeRequest&) = default;
};

#endif  // IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_MODEL_LOCATION_BAR_BADGE_REQUEST_H_
