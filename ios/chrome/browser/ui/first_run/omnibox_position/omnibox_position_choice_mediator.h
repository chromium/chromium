// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_OMNIBOX_POSITION_OMNIBOX_POSITION_CHOICE_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_OMNIBOX_POSITION_OMNIBOX_POSITION_CHOICE_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/first_run/omnibox_position/omnibox_position_choice_mutator.h"

namespace segmentation_platform {
class DeviceSwitcherResultDispatcher;
}  // namespace segmentation_platform

@protocol OmniboxPositionChoiceConsumer;
class PrefService;

/// Mediator that handles the omnibox position selection operations.
@interface OmniboxPositionChoiceMediator
    : NSObject <OmniboxPositionChoiceMutator>

/// The consumer for this object.
@property(nonatomic, weak) id<OmniboxPositionChoiceConsumer> consumer;
/// Pref service from the original browser state, used to set preferred omnibox
/// position.
@property(nonatomic, assign) PrefService* originalPrefService;
/// Device switcher result dispatcher, used to classify user as Safari switcher.
@property(nonatomic, assign)
    segmentation_platform::DeviceSwitcherResultDispatcher*
        deviceSwitcherResultDispatcher;

/// Initializes the mediator.
/// `isFirstRun`: The screen is shown during first run.
- (instancetype)initWithFirstRun:(BOOL)isFirstRun NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

/// Saves the selected omnibox position.
- (void)saveSelectedPosition;
/// Discards the selected omnibox position.
- (void)discardSelectedPosition;
/// Skip omnibox position selection. Sets the default omnibox position to the
/// position checked by default in the screen (the leading option). Only
/// available in FRE.
- (void)skipSelection;

@end

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_OMNIBOX_POSITION_OMNIBOX_POSITION_CHOICE_MEDIATOR_H_
