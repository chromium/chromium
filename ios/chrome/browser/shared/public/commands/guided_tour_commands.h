// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_GUIDED_TOUR_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_GUIDED_TOUR_COMMANDS_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"

// Different steps of the Guided Tour.
// LINT.IfChange(GuidedTourStep)
enum class GuidedTourStep {
  kNTP = 0,
  kTabGridIncognito = 1,
  kTabGridLongPress = 2,
  kTabGridTabGroup = 3,
  kMaxValue = kTabGridTabGroup,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ios/enums.xml:GuidedTourStep)

// Commands related to the Guided Tour.
@protocol GuidedTourCommands

// Indicates to the receiver that the spotlit view for `step` should be
// highlighted.
- (void)highlightViewInStep:(GuidedTourStep)step;

// Indicates to the receiver that the `step` has completed.
- (void)stepCompleted:(GuidedTourStep)step;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_GUIDED_TOUR_COMMANDS_H_
