// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_TIPS_TIPS_MODULE_STATE_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_TIPS_TIPS_MODULE_STATE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_module.h"

@protocol TipsModuleAudience;
@protocol TipsModuleConsumerSource;
namespace segmentation_platform {
enum class TipIdentifier;
}  // namespace segmentation_platform

// Helper class to contain the current Tips module state.
@interface TipsModuleState : MagicStackModule

// Initializes a `TipsModuleState` with `identifier`.
- (instancetype)initWithIdentifier:
    (segmentation_platform::TipIdentifier)identifier;

// Unique identifier for the given tip.
@property(nonatomic, readonly) segmentation_platform::TipIdentifier identifier;

// Serialized image data for the product image associated with the tip. Can be
// `nil`.
@property(nonatomic, readwrite) NSData* productImageData;

// The object that should handle user events.
@property(nonatomic, weak) id<TipsModuleAudience> audience;

// Tips model.
@property(nonatomic, strong) id<TipsModuleConsumerSource> consumerSource;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_TIPS_TIPS_MODULE_STATE_H_
