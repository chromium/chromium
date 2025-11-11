// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_MODEL_BADGE_TYPE_H_
#define IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_MODEL_BADGE_TYPE_H_

// Features that can be displayed as a badge in the location bar.
enum class LocationBarBadgeType {
  kNone = 0,
  // Badge type for the Save Passwords Infobar.
  kPasswordSave,
  // Badge type for the Update Passwords Infobar.
  kPasswordUpdate,
  // Badge type for the Incognito Badge.
  kIncognito,
  // Badge type for when there are more than one badge to be displayed.
  kOverflow,
  // Badge type for Save Credit Card Infobar.
  kSaveCard,
  // Badge type for the Translate Infobar.
  kTranslate,
  // Badge type for the Save Address Profile Infobar.
  kSaveAddressProfile,
  // Badge type for the Permissions Infobar with camera icon.
  kPermissionsCamera,
  // Badge type for the Permissions Infobar with microphone icon.
  kPermissionsMicrophone,
  // Badge type for the Contextual Panel entrypoint.
  kContextualPanelEntryPointSample,
  // Badge type for Price insight from Contextual Panel entrypoint.
  kPriceInsights,
  // Badge type for Reader mode from Contextual Panel entrypoint.
  kReaderMode,
  // Chip type for Gemini contextual cue chip.
  kGeminiContextualCueChip,
};

#endif  // IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_MODEL_BADGE_TYPE_H_
