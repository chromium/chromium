// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_TIPS_COORDINATOR_TIPS_NTP_CUSTOMIZATION_COORDINATOR_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_TIPS_COORDINATOR_TIPS_NTP_CUSTOMIZATION_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class TipsNTPCustomizationCoordinator;

// Delegate for `TipsNTPCustomizationCoordinator`.
@protocol TipsNTPCustomizationCoordinatorDelegate

// Indicates that the coordinator finished, with `didAccept` set to YES if the
// user tapped the primary action button.
- (void)tipsNTPCustomizationCoordinator:
            (TipsNTPCustomizationCoordinator*)coordinator
                    didFinishWithAccept:(BOOL)didAccept;

@end

// Coordinator presenting the actionable NTP Customization tip bottom sheet.
@interface TipsNTPCustomizationCoordinator : ChromeCoordinator

// Delegate receiving events from this coordinator.
@property(nonatomic, weak) id<TipsNTPCustomizationCoordinatorDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_TIPS_COORDINATOR_TIPS_NTP_CUSTOMIZATION_COORDINATOR_H_
