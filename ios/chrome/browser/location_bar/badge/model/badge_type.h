// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_MODEL_BADGE_TYPE_H_
#define IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_MODEL_BADGE_TYPE_H_

// Features that can be displayed as a badge in the location bar.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(LocationBarBadgeType)
enum class LocationBarBadgeType {
  kNone = 0,
  // Badge type for the Save Passwords Infobar.
  kPasswordSave = 1,
  // Badge type for the Update Passwords Infobar.
  kPasswordUpdate = 2,
  // Badge type for the Incognito Badge.
  kIncognito = 3,
  // Badge type for when there are more than one badge to be displayed.
  kOverflow = 4,
  // Badge type for Save Credit Card Infobar.
  kSaveCard = 5,
  // Badge type for the Translate Infobar.
  kTranslate = 6,
  // Badge type for the Save Address Profile Infobar.
  kSaveAddressProfile = 7,
  // Badge type for the Permissions Infobar with camera icon.
  kPermissionsCamera = 8,
  // Badge type for the Permissions Infobar with microphone icon.
  kPermissionsMicrophone,
  // Badge type for the Contextual Panel entrypoint.
  kContextualPanelEntryPointSample = 10,
  // Badge type for Price insight from Contextual Panel entrypoint.
  kPriceInsights = 11,
  // Badge type for Reader mode from Contextual Panel entrypoint.
  kReaderMode,
  // Chip type for Gemini contextual cue chip.
  kGeminiContextualCueChip = 13,

  kMaxValue = kGeminiContextualCueChip,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:IOSLocationBarBadgeType)

#endif  // IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_MODEL_BADGE_TYPE_H_
