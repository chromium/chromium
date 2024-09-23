// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_VIEW_CONTROLLER_AUDIENCE_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_VIEW_CONTROLLER_AUDIENCE_H_

#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_module_container_delegate.h"

enum class ContentSuggestionsModuleType;
enum class SafetyCheckItemType;
@class SetUpListItemView;

// Audience for the ContentSuggestions, getting information from it.
@protocol
    ContentSuggestionsViewControllerAudience <MagicStackModuleContainerDelegate>

// Notifies the audience of the UIKit viewWillDisappear: callback.
- (void)viewWillDisappear;

// Called when a Safety Check item is selected by the user.
- (void)didSelectSafetyCheckItem:(SafetyCheckItemType)type;

// Indicates that the user has tapped the given `view`.
- (void)didTapSetUpListItemView:(SetUpListItemView*)view;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_VIEW_CONTROLLER_AUDIENCE_H_
