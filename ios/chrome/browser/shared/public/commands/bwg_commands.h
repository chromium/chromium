// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_BWG_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_BWG_COMMANDS_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_entry_flow_result.h"

namespace signin_metrics {
enum class AccessPoint;
}  // namespace signin_metrics

namespace gemini {
enum class EntryPoint;
enum class FloatyUpdateSource;
}  // namespace gemini

@class GeminiStartupState;

namespace web {
class WebState;
}  // namespace web

// Commands relating to the BWG flow.
@protocol BWGCommands <NSObject>

// Starts the Gemini flow with the given startup state.
- (void)startGeminiFlowWithStartupState:(GeminiStartupState*)startupState;

// Dismiss the Gemini flow with a completion block.
- (void)dismissGeminiFlowWithCompletion:(ProceduralBlock)completion;

// Attempts to display the automatic BWG promo depending on whether the active
// web state is eligible. If the page is ineligible, does nothing.
- (void)showBWGPromoIfPageIsEligible;

// Handles hiding the Gemini floaty from an update `source`. When in a hidden
// state, the floaty still persists in memory and needs to be properly cleaned
// up.
- (void)hideFloatyIfInvokedAnimated:(BOOL)animated
                         fromSource:(gemini::FloatyUpdateSource)source;

// Updates Gemini floaty's visibility based on eligibility from an update
// `source`. Can be used to re-show an invoked Gemini floaty or hide the floaty
// for ineligible sites.
- (void)updateFloatyVisibilityIfEligibleAnimated:(BOOL)animated
                                      fromSource:
                                          (gemini::FloatyUpdateSource)source;

// Updates the Gemini floaty with a trait collection change.
- (void)updateFloatyWithTraitCollection:(UITraitCollection*)traitCollection;

// Starts the FRE flow with a completion block.
- (void)startGeminiFREWithCompletion:(void (^)(BOOL success))completion
                      fromEntryPoint:(gemini::EntryPoint)entryPoint;

// Starts the full Gemini entry flow and checks sign-in state, handles
// eligibility, and starts a Gemini session.
// - `startupState`: Entry point and configuration for the Gemini session.
// - `baseViewController`: The view controller to present sign-in and
//   account menu from.
// - `accessPoint`: The sign-in access point used for sign-in metrics
//   (e.g., signin_metrics::AccessPoint::kIosPageActionMenu). Only used
//   if the user is signed out and sign-in is triggered.
// - `showSnackbarOnCompletion`: Whether to show a snackbar when the flow
//   completes with an ineligible state (e.g., page not eligible, account
//   restricted).
// - `completion`: Called with the final result of the flow. Pass nil if
//   the result is not needed.
- (void)
    startGeminiEntryFlowWithStartupState:(GeminiStartupState*)startupState
                      baseViewController:(UIViewController*)baseViewController
                             accessPoint:
                                 (signin_metrics::AccessPoint)accessPoint
                showSnackbarOnCompletion:(BOOL)showSnackbar
                              completion:(GeminiEntryFlowCompletion)completion;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_BWG_COMMANDS_H_
