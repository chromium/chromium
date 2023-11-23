// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_OMNIBOX_POSITION_OMNIBOX_POSITION_CHOICE_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_OMNIBOX_POSITION_OMNIBOX_POSITION_CHOICE_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/first_run/omnibox_position/omnibox_position_choice_mutator.h"

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

/// Saves the selected omnibox position.
- (void)saveSelectedPosition;
/// Discards the selected omnibox position.
- (void)discardSelectedPosition;

@end

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_OMNIBOX_POSITION_OMNIBOX_POSITION_CHOICE_MEDIATOR_H_
