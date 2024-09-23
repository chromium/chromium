// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_MAGIC_STACK_MAGIC_STACK_MODULE_CONTAINER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_MAGIC_STACK_MAGIC_STACK_MODULE_CONTAINER_DELEGATE_H_

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

// Indicates that the user has tapped the context menu item to enable
// notifications.
- (void)enableNotifications:(ContentSuggestionsModuleType)type;

// Indicates that the user has tapped the context menu item to disable
// notifications.
- (void)disableNotifications:(ContentSuggestionsModuleType)type;

// Indicates that the user has tapped the context menu item to edit the Magic
// Stack modules.
- (void)customizeCardsWasTapped;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_MAGIC_STACK_MAGIC_STACK_MODULE_CONTAINER_DELEGATE_H_
