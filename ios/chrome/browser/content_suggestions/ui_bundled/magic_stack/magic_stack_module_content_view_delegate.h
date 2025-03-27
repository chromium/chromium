// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_MAGIC_STACK_MAGIC_STACK_MODULE_CONTENT_VIEW_DELEGATE_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_MAGIC_STACK_MAGIC_STACK_MODULE_CONTENT_VIEW_DELEGATE_H_

enum class ContentSuggestionsModuleType;
@class UIAction;

// Interface for listening to events occurring in MagicStackModuleContainer.
@protocol MagicStackModuleContentViewDelegate

// Sets the subtitle text of the Magic Stack Module Container to `subtitle`.
- (void)setSubtitle:(NSString*)subtitle;

// Updates the visibility of the notifications opt-in button within the Magic
// Stack Module Container based on `showNotificationsOptIn`.
- (void)updateNotificationsOptInVisibility:(BOOL)showNotificationsOptIn;

// Updates the visibility of the separator line within the Magic Stack
// Module Container based on `isHidden`.
- (void)updateSeparatorVisibility:(BOOL)isHidden;

// Notifies the delegate that the context menu interaction will end display.
- (void)notifyContextMenuInteractionEndWithAnimator:
    (id<UIContextMenuInteractionAnimating>)animator;

// Returns the context menu elements for the current module.
- (NSArray<UIMenuElement*>*)contextMenuElementsForCurrentModule;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_MAGIC_STACK_MAGIC_STACK_MODULE_CONTENT_VIEW_DELEGATE_H_
