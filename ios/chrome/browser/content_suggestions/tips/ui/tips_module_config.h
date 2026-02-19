// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_TIPS_UI_TIPS_MODULE_CONFIG_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_TIPS_UI_TIPS_MODULE_CONFIG_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/content_suggestions/ui/cells/icon_detail_view.h"
#import "ios/chrome/browser/content_suggestions/ui/cells/icon_detail_view_config.h"

@protocol TipsModuleAudience;

namespace segmentation_platform {
enum class TipIdentifier;
}  // namespace segmentation_platform

// Helper class to contain the current Tips module state.
@interface TipsModuleConfig : IconDetailViewConfig <IconDetailViewTapDelegate>

// The updates to properties must be reflected in the copy method.
// LINT.IfChange(Copy)
// Unique identifier for the given tip.
@property(nonatomic, readonly) segmentation_platform::TipIdentifier identifier;

// Serialized image data for the product image associated with the tip. Can be
// `nil`.
@property(nonatomic, readwrite) NSData* productImageData;

// The object that should handle user events.
@property(nonatomic, weak) id<TipsModuleAudience> audience;

// Initializes a `TipsModuleConfig` with `identifier`.
- (instancetype)initWithTipIdentifier:
    (segmentation_platform::TipIdentifier)identifier;
// LINT.ThenChange(tips_module_config.mm:Copy)

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_TIPS_UI_TIPS_MODULE_CONFIG_H_
