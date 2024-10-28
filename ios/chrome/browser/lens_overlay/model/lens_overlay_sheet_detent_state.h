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
};

// Indicates the state of the bottom sheet dimension.
typedef NS_ENUM(NSUInteger, SheetDimensionState) {
  // The bottom sheet is not shown.
  SheetDimensionStateHidden = 0,
  // The bottom sheet is expanded.
  SheetDimensionStateLarge = 1,
  // The bottom sheet is covering approximately half of the screen.
  SheetDimensionStateMedium = 2,
  // The bottom sheet is in peaking state.
  SheetDimensionStatePeaking = 3,
  // The bottom sheet is displayed with the fixed custom dimensions for consent.
  SheetDimensionStateConsent = 4,
};

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_SHEET_DETENT_STATE_H_
