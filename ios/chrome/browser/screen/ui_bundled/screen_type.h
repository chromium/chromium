// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SCREEN_UI_BUNDLED_SCREEN_TYPE_H_
#define IOS_CHROME_BROWSER_SCREEN_UI_BUNDLED_SCREEN_TYPE_H_

// The types of the start up screens.
// TODO(crbug.com/422216784): Rename to include post first-run actions.
typedef NS_ENUM(NSInteger, ScreenType) {
  kSignIn,
  kHistorySync,
  kDefaultBrowserPromo,
  kChoice,
  kDockingPromo,
  kBestFeatures,
  kLensInteractivePromo,
  kLensAnimatedPromo,
  // Actions that are performed post first-run experience.
  kSyncedSetUp,
  kGuidedTour,
  kSafariImport,
  // It isn't a screen, but a signal that no more screen should be
  // presented.
  kStepsCompleted,
};

#endif  // IOS_CHROME_BROWSER_SCREEN_UI_BUNDLED_SCREEN_TYPE_H_
