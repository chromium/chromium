// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_MAGIC_STACK_MAGIC_STACK_MODULE_CONTAINER_DELEGATE_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_MAGIC_STACK_MAGIC_STACK_MODULE_CONTAINER_DELEGATE_H_

#import <UIKit/UIKit.h>

enum class ContentSuggestionsModuleType;

// Protocol asking the receiver for more contextual information about modules.
@protocol MagicStackModuleContainerDelegate

// Indicates to the receiver that the "See More" button was tapped in the
// module.
- (void)seeMoreWasTappedForModuleType:(ContentSuggestionsModuleType)type;

// Indicates to the receiver that the module of `type` should be never shown
// anymore.
- (void)neverShowModuleType:(ContentSuggestionsModuleType)type;

// Indicates that the user has enabled notifications. The source of the action
// is specified by the `viaContextMenu` parameter, which is YES if the user
// enabled notifications via the context menu, and NO if the action originated
// from the top-right action button.
- (void)enableNotifications:(ContentSuggestionsModuleType)type
             viaContextMenu:(BOOL)viaContextMenu;

// Indicates that the user has disabled notifications. The source of the action
// is specified by the `viaContextMenu` parameter, which is YES if the user
// disabled notifications via the context menu, and NO if the action originated
// from the top-right action button.
- (void)disableNotifications:(ContentSuggestionsModuleType)type
              viaContextMenu:(BOOL)viaContextMenu;

// Indicates that the user has tapped the context menu item to edit the Magic
// Stack modules.
- (void)customizeCardsWasTapped;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_MAGIC_STACK_MAGIC_STACK_MODULE_CONTAINER_DELEGATE_H_
