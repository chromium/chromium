// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_BWG_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_BWG_COMMANDS_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"

namespace bwg {
enum class EntryPoint;
}

// Commands relating to the BWG flow.
@protocol BWGCommands

// Starts the Gemini flow with an entry point.
- (void)startGeminiFlowWithEntryPoint:(bwg::EntryPoint)entryPoint;

// Starts the Gemini flow with a provided image as attachment.
- (void)startGeminiFlowWithImageAttachment:(UIImage*)image
                                entryPoint:(bwg::EntryPoint)entryPoint;

// Dismiss the Gemini flow with a completion block.
- (void)dismissGeminiFlowWithCompletion:(ProceduralBlock)completion;

// Attempts to display the automatic BWG promo depending on whether the active
// web state is eligible. If the page is ineligible, does nothing.
- (void)showBWGPromoIfPageIsEligible;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_BWG_COMMANDS_H_
