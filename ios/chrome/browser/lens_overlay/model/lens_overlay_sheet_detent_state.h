// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_SHEET_DETENT_STATE_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_SHEET_DETENT_STATE_H_

// Indicates the state of the bottom sheet detents.
typedef NS_ENUM(NSUInteger, SheetDetentState) {
  // The bottom sheet is free to oscillate between medium and large.
  SheetDetentStateUnrestrictedMovement,
  // The bottom sheet is presenting the consent dialog sheet.
  SheetDetentStateConsentDialog,
  // The bottom sheet is in the peak state.
  SheetDetentStatePeakEnabled,
  // The bottom sheet is showing an info message.
  SheetDetentStateInfoMessage,
};

// Indicates the state of the bottom sheet dimension.
// These values are logged to UMA. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(SheetDimensionState)
enum class SheetDimensionState {
  // The bottom sheet is not shown.
  kHidden = 0,
  // The bottom sheet is expanded.
  kLarge = 1,
  // The bottom sheet is covering approximately half of the screen.
  kMedium = 2,
  // The bottom sheet is in peaking state.
  kPeaking = 3,
  // The bottom sheet is displayed with the fixed custom dimensions for consent.
  kConsent = 4,
  // The bottom sheet is showing an info message.
  kInfoMessage = 5,
  kMaxValue = kInfoMessage,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/lens/enums.xml:SheetDimensionState)

// Indicates the presentation strategy of the bottom sheet medium detent based
// on the current Lens filter.
typedef NS_ENUM(NSUInteger, SheetDetentPresentationStategy) {
  // The UI is in selection mode, with the focus on the search result.
  SheetDetentPresentationStategySelection,
  // The UI is in translate mode, with the focus on the translated image.
  SheetDetentPresentationStategyTranslate,
};

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_SHEET_DETENT_STATE_H_
