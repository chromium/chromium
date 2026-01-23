// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_BWG_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_BWG_COMMANDS_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"

namespace gemini {
enum class EntryPoint;
}  // namespace gemini

namespace web {
class WebState;
}  // namespace web

// Commands relating to the BWG flow.
@protocol BWGCommands <NSObject>

// Starts the Gemini flow with an entry point.
- (void)startGeminiFlowWithEntryPoint:(gemini::EntryPoint)entryPoint;

// Starts the Gemini flow with a provided image as attachment.
- (void)startGeminiFlowWithImageAttachment:(UIImage*)image
                                entryPoint:(gemini::EntryPoint)entryPoint;

// Dismiss the Gemini flow with a completion block.
- (void)dismissGeminiFlowWithCompletion:(ProceduralBlock)completion;

// Attempts to display the automatic BWG promo depending on whether the active
// web state is eligible. If the page is ineligible, does nothing.
- (void)showBWGPromoIfPageIsEligible;

// Hide Gemini floaty. When in a hidden state, the floaty still persists in
// memory and needs to be properly cleaned up.
- (void)hideFloatyIfInvokedAnimated:(BOOL)animated;

// Show Gemini floaty. Used to re-show an invoked Gemini floaty.
- (void)showFloatyIfInvokedAnimated:(BOOL)animated;

// Updates floaty visibility when persisting across WebStates.
- (void)updateFloatyVisibilityForWebState:(web::WebState*)webState;

- (void)updateFloatyWithTraitCollection:(UITraitCollection*)traitCollection;

// Starts the FRE flow with a completion block.
- (void)startGeminiFREWithCompletion:(void (^)(BOOL success))completion
                      fromEntryPoint:(gemini::EntryPoint)entryPoint;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_BWG_COMMANDS_H_
