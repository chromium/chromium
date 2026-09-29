// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_MENU_UI_COMPOSEBOX_MENU_CONTEXT_MENU_BUILDER_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_MENU_UI_COMPOSEBOX_MENU_CONTEXT_MENU_BUILDER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_mutator.h"
#import "ios/chrome/browser/composebox/ui/composebox_ui_input_state.h"

// Builder for the contextual composebox menu.
@interface ComposeboxMenuContextMenuBuilder : NSObject

// The mutator for this menu UI.
@property(nonatomic, weak) id<ComposeboxMenuMutator> mutator;

// Instantiates a new builder based on the given input state.
- (instancetype)initWithInputState:(ComposeboxUIInputState*)inputState;

// Creates a new UIMenu instance based on the given input state.
- (UIMenu*)createMenu;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_MENU_UI_COMPOSEBOX_MENU_CONTEXT_MENU_BUILDER_H_
