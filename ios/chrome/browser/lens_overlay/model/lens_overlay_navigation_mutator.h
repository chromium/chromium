// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_NAVIGATION_MUTATOR_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_NAVIGATION_MUTATOR_H_

#import <Foundation/Foundation.h>

@protocol ChromeLensOverlayResult;

/// Protocol to mutate navigation in the lens overlay.
@protocol LensOverlayNavigationMutator <NSObject>

/// Displays the provided Lens `result` in the overlay.
- (void)loadLensResult:(id<ChromeLensOverlayResult>)result;

/// Initiates a refresh of the Lens `result` with an updated query and prepares
/// the UI. This function triggers the Lens API to generate a new
/// `ChromeLensOverlayResult` derived from the provided `result`. It also
/// performs necessary UI updates in anticipation of the new result, which may
/// be displayed by a later `loadLensResult:` call.
- (void)reloadLensResult:(id<ChromeLensOverlayResult>)result;

/// Loads `URL` in the overlay.
- (void)reloadURL:(GURL)URL;

/// Called when the navigation list has been updated. This function is triggered
/// whenever the navigation history changes, potentially altering the
/// availability of navigating to previous entry.
- (void)onBackNavigationAvailabilityMaybeChanged:(BOOL)canGoBack;

@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_NAVIGATION_MUTATOR_H_
