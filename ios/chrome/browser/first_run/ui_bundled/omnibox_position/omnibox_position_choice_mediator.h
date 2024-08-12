// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_OMNIBOX_POSITION_OMNIBOX_POSITION_CHOICE_MEDIATOR_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_OMNIBOX_POSITION_OMNIBOX_POSITION_CHOICE_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/first_run/ui_bundled/omnibox_position/omnibox_position_choice_mutator.h"

namespace segmentation_platform {
class DeviceSwitcherResultDispatcher;
}  // namespace segmentation_platform

@protocol OmniboxPositionChoiceConsumer;

/// Mediator that handles the omnibox position selection operations.
@interface OmniboxPositionChoiceMediator
    : NSObject <OmniboxPositionChoiceMutator>

/// The consumer for this object.
@property(nonatomic, weak) id<OmniboxPositionChoiceConsumer> consumer;
/// Device switcher result dispatcher, used to classify user as Safari switcher.
@property(nonatomic, assign)
    segmentation_platform::DeviceSwitcherResultDispatcher*
        deviceSwitcherResultDispatcher;

/// Saves the selected omnibox position.
- (void)saveSelectedPosition;
/// Discards the selected omnibox position.
- (void)discardSelectedPosition;

@end

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_OMNIBOX_POSITION_OMNIBOX_POSITION_CHOICE_MEDIATOR_H_
