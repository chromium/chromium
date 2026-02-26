// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_BWG_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_BWG_COMMANDS_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"

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

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_BWG_COMMANDS_H_
