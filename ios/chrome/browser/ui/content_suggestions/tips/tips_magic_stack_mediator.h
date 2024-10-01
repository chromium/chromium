// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_TIPS_TIPS_MAGIC_STACK_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_TIPS_TIPS_MAGIC_STACK_MEDIATOR_H_

#import <UIKit/UIKit.h>

@class TipsModuleState;
namespace segmentation_platform {
enum class TipIdentifier;
}  // namespace segmentation_platform

// Mediator for managing the state of the Tips (Magic Stack) module.
@interface TipsMagicStackMediator : NSObject

// Used by the Tips module for the current module state.
@property(nonatomic, strong, readonly) TipsModuleState* state;

// Default initializer.
- (instancetype)initWithIdentifier:
    (segmentation_platform::TipIdentifier)identifier NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Reconfigures `TipsMagicStackMediator` with a new tip `identifier`.
- (void)reconfigureWithTipIdentifier:
    (segmentation_platform::TipIdentifier)identifier;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_TIPS_TIPS_MAGIC_STACK_MEDIATOR_H_
