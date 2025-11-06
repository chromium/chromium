// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_INPUT_PLATE_CONSUMER_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_INPUT_PLATE_CONSUMER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/composebox/ui/composebox_input_item.h"

// Consumer for the composebox composebox.
@protocol ComposeboxInputPlateConsumer

// Sets the current list of items to be displayed.
- (void)setItems:(NSArray<ComposeboxInputItem*>*)items;

// Updates the state for the item with the given token.
- (void)updateState:(ComposeboxInputItemState)state
    forItemWithToken:(const base::UnguessableToken&)token;

// Updates the mic and lens button visibility.
- (void)hideLensAndMicButton:(BOOL)hidden;

// Updates the send button visibility.
- (void)hideSendButton:(BOOL)hidden;

// Sets whether the "Attach current tab" action is enabled.
- (void)setCanAttachTabAction:(BOOL)canAttachTabAction;

// Sets whether AI mode is enabled.
- (void)setAIModeEnabled:(BOOL)AIModeEnabled;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_INPUT_PLATE_CONSUMER_H_
