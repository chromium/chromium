// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_MAGIC_STACK_MAGIC_STACK_CONTEXT_MENU_INTERACTION_HANDLER_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_MAGIC_STACK_MAGIC_STACK_CONTEXT_MENU_INTERACTION_HANDLER_H_

#import <UIKit/UIKit.h>

enum class ContentSuggestionsModuleType;
@class MagicStackModule;
@protocol MagicStackModuleContainerDelegate;

/// Object that handles context menu interactions on the magic stack module..
@interface MagicStackContextMenuInteractionHandler
    : NSObject <UIContextMenuInteractionDelegate>

/// Configure the interaction handler with type.
- (void)configureWithType:(ContentSuggestionsModuleType)type
                   config:(MagicStackModule*)config;

/// Menu elements being shown on interaction.
- (NSArray<UIMenuElement*>*)menuElements;

/// Resets the handler to original state.
- (void)reset;

/// Delegate for this module.
@property(nonatomic, weak) id<MagicStackModuleContainerDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_MAGIC_STACK_MAGIC_STACK_CONTEXT_MENU_INTERACTION_HANDLER_H_
