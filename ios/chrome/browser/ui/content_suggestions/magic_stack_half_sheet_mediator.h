// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_MAGIC_STACK_HALF_SHEET_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_MAGIC_STACK_HALF_SHEET_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/content_suggestions/magic_stack_half_sheet_model_delegate.h"

@protocol MagicStackHalfSheetConsumer;
class PrefService;

// Mediator for the Magic Stack Half Sheet.
@interface MagicStackHalfSheetMediator
    : NSObject <MagicStackHalfSheetModelDelegate>

// Initializes this class with the appropriate localState.
- (instancetype)initWithPrefService:(PrefService*)prefService;

// Disconnects the mediator.
- (void)disconnect;

// Consumer for this mediator.
@property(nonatomic, weak) id<MagicStackHalfSheetConsumer> consumer;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_MAGIC_STACK_HALF_SHEET_MEDIATOR_H_
