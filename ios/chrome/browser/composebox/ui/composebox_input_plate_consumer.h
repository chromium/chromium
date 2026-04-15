// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_INPUT_PLATE_CONSUMER_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_INPUT_PLATE_CONSUMER_H_

#import <UIKit/UIKit.h>

#import <unordered_set>

#import "ios/chrome/browser/composebox/ui/composebox_input_item.h"

enum class ComposeboxInputPlateControls : unsigned int;
enum class ComposeboxModelOption;
@class ComposeboxServerStrings;
@class ComposeboxUIInputState;

// Consumer for the composebox composebox.
@protocol ComposeboxInputPlateConsumer

// Sets the current list of items to be displayed.
- (void)setItems:(NSArray<ComposeboxInputItem*>*)items;

// Updates the state for the item with the given identifier.
- (void)updateState:(ComposeboxInputItemState)state
    forItemWithIdentifier:(const base::UnguessableToken&)identifier;

// Sets whether to show the shortcuts (Lens and microphone actions).
- (void)updateVisibleControls:(ComposeboxInputPlateControls)controls;

// Whether to present in compact (single line) mode.
- (void)setCompact:(BOOL)compact;

// Sets the UI input state.
- (void)setUIInputState:(ComposeboxUIInputState*)state;

// Called when the text field height changes.
- (void)updatePreferredContentSizeForNewTextFieldHeight;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_INPUT_PLATE_CONSUMER_H_
