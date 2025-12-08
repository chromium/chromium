// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_INPUT_PLATE_CONSUMER_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_INPUT_PLATE_CONSUMER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/composebox/ui/composebox_input_item.h"

enum class ComposeboxInputPlateControls : unsigned int;

// Consumer for the composebox composebox.
@protocol ComposeboxInputPlateConsumer

// Sets the current list of items to be displayed.
- (void)setItems:(NSArray<ComposeboxInputItem*>*)items;

// Updates the state for the item with the given identifier.
- (void)updateState:(ComposeboxInputItemState)state
    forItemWithIdentifier:(const base::UnguessableToken&)identifier;

// Sets whether to show the shortcuts (Lens and microphone actions).
- (void)updateVisibleControls:(ComposeboxInputPlateControls)controls;

// Sets whether AI mode is enabled.
- (void)setAIModeEnabled:(BOOL)enabled;

// Sets whether Image Generation mode is enabled.
- (void)setImageGenerationEnabled:(BOOL)enabled;

// Whether to present in compact (single line) mode.
- (void)setCompact:(BOOL)compact;

// Sets the favicon for the current tab.
- (void)setCurrentTabFavicon:(UIImage*)favicon;

// Sets whether the "Attach current tab" action is hidden.
- (void)hideAttachCurrentTabAction:(BOOL)hidden;

// Sets whether the attach tab actions are hidden.
- (void)hideAttachTabActions:(BOOL)hidden;

// Sets whether the attach tab actions are disabled.
- (void)disableAttachTabActions:(BOOL)disabled;

// Sets whether the attach file actions are hidden.
- (void)hideAttachFileActions:(BOOL)hidden;

// Sets whether the attach file actions are disabled.
- (void)disableAttachFileActions:(BOOL)disabled;

// Sets whether the create image actions are hidden.
- (void)hideCreateImageActions:(BOOL)hidden;

// Sets whether the create image actions are disabled.
- (void)disableCreateImageActions:(BOOL)disabled;

// Sets whether the camera actions are disabled.
- (void)disableCameraActions:(BOOL)disabled;

// Sets whether the gallery actions are disabled.
- (void)disableGalleryActions:(BOOL)disabled;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_INPUT_PLATE_CONSUMER_H_
