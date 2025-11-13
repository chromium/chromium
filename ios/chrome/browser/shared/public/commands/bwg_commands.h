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

// Starts the BWG flow.
- (void)startBWGFlowWithEntryPoint:(bwg::EntryPoint)entryPoint;

// Starts the BWG flow with a provided image as attachment.
- (void)startBWGFlowWithImageAttachment:(UIImage*)image
                             entryPoint:(bwg::EntryPoint)entryPoint;

// Dismiss the BWG flow with a completion block.
- (void)dismissBWGFlowWithCompletion:(ProceduralBlock)completion;

// Attempts to display the automatic BWG promo depending on whether the active
// web state is eligible. If the page is ineligible, does nothing.
- (void)showBWGPromoIfPageIsEligible;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_BWG_COMMANDS_H_
