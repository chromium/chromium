// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SCREEN_UI_BUNDLED_SCREEN_TYPE_H_
#define IOS_CHROME_BROWSER_SCREEN_UI_BUNDLED_SCREEN_TYPE_H_

// The types of the start up screens.
// Values are persisted in `prefs::kFirstRunDeferredScreens`; entries should not
// be renumbered and numeric values should never be reused.
// TODO(crbug.com/422216784): Rename to include post first-run actions.
enum class ScreenType {
  kSignIn = 0,
  kHistorySync = 1,
  kDefaultBrowserPromo = 2,
  kChoice = 3,
  kBestFeatures = 4,
  kLensInteractivePromo = 5,
  kLensAnimatedPromo = 6,
  // Actions that are performed post first-run experience.
  kSyncedSetUp = 7,
  kGuidedTour = 8,
  kSafariImport = 9,
  // It isn't a screen, but a signal that no more screen should be
  // presented.
  kStepsCompleted = 10,
};

#endif  // IOS_CHROME_BROWSER_SCREEN_UI_BUNDLED_SCREEN_TYPE_H_
