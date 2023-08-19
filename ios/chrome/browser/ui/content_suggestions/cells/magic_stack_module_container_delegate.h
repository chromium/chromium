// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_MAGIC_STACK_MODULE_CONTAINER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_MAGIC_STACK_MODULE_CONTAINER_DELEGATE_H_

enum class ContentSuggestionsModuleType;

// Protocol asking the receiver for more contextual information about modules.
@protocol MagicStackModuleContainerDelegate

// YES if the module of `type` is the only module being shown in the Magic
// Stack.
- (BOOL)doesMagicStackShowOnlyOneModule:(ContentSuggestionsModuleType)type;

// Indicates to the receiver that the "See More" button was tapped in the
// module.
- (void)seeMoreWasTappedForModuleType:(ContentSuggestionsModuleType)type;

// Indicates to the receiver that the module of `type` should be never shown
// anymore.
- (void)neverShowModuleType:(ContentSuggestionsModuleType)type;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_MAGIC_STACK_MODULE_CONTAINER_DELEGATE_H_
